#include "reactor.h"

using namespace reactor;

class TestServer : public NetServer {
public:
    TestServer(void) {
        start_time = last_time = 0;
    }
    ~TestServer(void) {}

    virtual int handle_msg(client_id_t cid, basic::ByteBuffer &buffer) {
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
        LOG_TRACE("server client disconnected[cid: %d]", cid);
        return 0;
    }

private:
    uint64_t recv_size = 0;
    os::mtime_t start_time;
    os::mtime_t last_time;
};

int main(int argc, char **argv)
{
    TestServer server;
    server.start("192.168.0.103", 12138, ptl::ProtocolType_Raw);

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        }
    }


    return 0;
}