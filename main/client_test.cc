#include "reactor.h"

using namespace reactor;

class TestClient : public NetClient {
public:
    TestClient(void) {}
    ~TestClient(void) {}

    int handle_msg(basic::ByteBuffer &buffer) {
        LOG_TRACE("server_response: %s", buffer.str().c_str());
        return 0;
    }

    int notify_client_disconnected(client_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }
};

int main(int argc, char **argv)
{
    TestClient client;
    client.connect("raw://192.168.0.103:12138");

    uint32_t send_gap = 2; // 单位：ms
    uint64_t send_size = 100;
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
            os::mtime_t start_time = os::Time::now();
            for (int i = 0; i < send_counts; ++i) {
                client.send_data(buffer);
                os::Time::sleep(send_gap);
            }
            uint64_t total_size = send_size * send_counts;
            LOG_GLOBAL_TRACE("send over![send_total_size: %ld bytes, send_speed: %lf bytes/ms]", total_size, static_cast<double>(total_size) / (os::Time::now() - start_time));
        }
    }

    return 0;
}