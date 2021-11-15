#include "reactor.h"

using namespace reactor;

class TestClient : public NetClient {
public:
    TestClient(void) {}
    ~TestClient(void) {}

    int handle_msg(basic::ByteBuffer &buffer) {
        if (os::Time::now() - last_time > 3000) {
            start_time = os::Time::now();
            last_time = start_time;
            recv_size = 0;
        }
        recv_size += buffer.data_size();
        if (os::Time::now() - last_time > 2000) {
            LOG_TRACE("recv bytes: %ld bytes, recv_speed: %lf bytes/ms", recv_size, (double)recv_size / (os::Time::now() - start_time));
            last_time = os::Time::now();
        }
        return 0;
    }

    int notify_client_disconnected(client_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }

private:
    uint64_t recv_size = 0;
    os::mtime_t start_time;
    os::mtime_t last_time;
};

int main(int argc, char **argv)
{
    TestClient client;
    client.connect("tcp://192.168.0.103:12138");

    uint32_t send_gap = 2; // 单位：ms
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