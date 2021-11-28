#include "reactor.h"

using namespace reactor;

class TestClient : public HttpNetClient {
public:
    TestClient(void) {}
    ~TestClient(void) {}

    int handle_msg(ptl::HttpPtl &http_ptl) {
        basic::ByteBuffer buffer;
        http_ptl.generate(buffer);
        std::cout << buffer.str() << std::endl << std::endl;

        disconnect();
        return 0;
    }

    int notify_client_disconnected(client_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }

    int request_http(void) {
        ptl.set_request(HTTP_METHOD_GET, url_); // 服务端返回是 /Response
        send_data(ptl);
        return 0;
    }
private:
    ptl::HttpPtl ptl;
    ByteBuffer request_buffer;
    ByteBuffer response_buffer;
};

int main(int argc, char **argv)
{
    ReactorConfig_t rconfig;
    rconfig.max_wait_task = 10000;
    rconfig.threads_num = 5;
    rconfig.send_thread_num = 1;
    reactor_start(rconfig);

    TestClient client;

    uint32_t send_gap = 500; // 单位：ms
    uint64_t send_size = 400;
    uint64_t send_counts = 10000;
    std::string str;
    for (int i = 0; i < send_size; ++i) {
        str += 'H';
    }
    basic::ByteBuffer buffer(str);

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        } else if (ch == 's') {
            client.connect("http://fundgz.1234567.com.cn/js/007531.js");
        }
    }

    reactor_stop();
    return 0;
}