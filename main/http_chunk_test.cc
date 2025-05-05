/*
    测试http想http服务器请求数据
*/
#include "reactor.h"

using namespace reactor;

class TestClient : public HttpNetClient {
public:
    TestClient(void) {}
    ~TestClient(void) {}

    virtual int handle_msg(ptl::HttpPtl &http_ptl, ptl::HttpParse_ErrorCode err) {
        basic::ByteBuffer data_buffer;
        if (http_ptl.get_status_code() == 200) {
            if (http_ptl.is_tranfer_encode()) {
                std::vector<basic::ByteBuffer> &datas = http_ptl.get_tranfer_encode_datas();
                if (datas.size() > 0) {
                    for (std::size_t i = 0; i < datas.size(); ++i) {
                        data_buffer += datas[i];
                    }
                    datas.clear();
                }
            } else {
                data_buffer = http_ptl.get_content();
            }
        } else {
            LOG_GLOBAL_INFO("Parse http error: %d\n%s",  http_ptl.get_status_code(), http_ptl.get_content().str().c_str());
        }
        LOG_GLOBAL_INFO("HttpErr: %d\n", err);

        if (data_buffer.data_size() > 0) {
            LOG_GLOBAL_INFO("Data: \n%s\n\n", data_buffer.str().c_str());
        }
        return 0;
    }

    int notify_client_disconnected(sock_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }

    int request_http(void) {
        ptl.set_request(HTTP_METHOD_GET, "/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?symbol=sh600084&scale=240&ma=25&datalen=1024");
        ptl.set_header_option(HTTP_HEADER_UserAgent, "WeHttp/1.0");
        ptl.set_header_option(HTTP_HEADER_Host, "money.finance.sina.com.cn");
        ptl.set_header_option(HTTP_HEADER_Connection, "Keep-Alive");
        ptl.set_header_option(HTTP_HEADER_ContentType, "application/json; charset=gbk");

        send_data(ptl);
        basic::ByteBuffer print_buffer;
        ptl.generate(print_buffer);
        LOG_GLOBAL_INFO("\n%s", print_buffer.str().c_str());
        ptl.clear();

        return 0;
    }
private:
    ptl::HttpPtl ptl;
};

int main(int argc, char **argv)
{
    ReactorConfig_t rconfig;
    reactor_start(rconfig);

    TestClient client;
    // 先按s链接到服务器，然后按r发送请求
    while (true) {
        int ch = getchar();
        if (ch == 'q') {
            reactor_stop();
            break;
        } else if (ch == 's') {
            if (client.connect("http://money.finance.sina.com.cn") < 0) {
                LOG_GLOBAL_INFO("Connect url failed!");
                return -1;
            }
        } else if (ch == 'r') {
            client.request_http();
        }
    }

    return 0;
}