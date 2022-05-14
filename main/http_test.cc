#include "reactor.h"

using namespace reactor;

class TestClient : public HttpNetClient {
public:
    TestClient(void) {}
    ~TestClient(void) {}

    int handle_msg(ptl::HttpPtl &http_ptl, ptl::HttpParse_ErrorCode err) {
        basic::ByteBuffer buffer;
        http_ptl.generate(buffer);
        LOG_GLOBAL_INFO("HttpErr: %d\n", err);
        LOG_GLOBAL_INFO("Data: \n%s\n\n", buffer.str().c_str());

        return 0;
    }

    int notify_client_disconnected(client_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }

    int request_http(void) {
        ptl.set_request(HTTP_METHOD_GET, url_parser_.res_path_);
        ptl.set_header_option(HTTP_HEADER_UserAgent, "WeHttp/1.0");
        ptl.set_header_option(HTTP_HEADER_Host, "fundgz.1234567.com.cn");
        ptl.set_header_option(HTTP_HEADER_Connection, "Keep-Alive");

        send_data(ptl);
        ptl.clear();

        return 0;
    }
private:
    ptl::HttpPtl ptl;
};

int main(int argc, char **argv)
{
    ReactorConfig_t rconfig;
    rconfig.max_wait_task = 10000;
    rconfig.threads_num = 5;
    rconfig.send_thread_num = 1;
    reactor_start(rconfig);

    TestClient client;
    
    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        } else if (ch == 's') {
            client.connect("http://fundgz.1234567.com.cn/js/161725.js");
        } else if (ch == 'r') {
            client.request_http();
        }
    }

    reactor_stop();
    return 0;
}