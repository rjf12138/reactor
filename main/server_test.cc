#include "reactor.h"

using namespace reactor;

class TestServer : public NetServer {
public:
    TestServer(void) {}
    ~TestServer(void) {}

    virtual int handle_msg(client_id_t cid, basic::ByteBuffer &buffer) {
        if (is_first == false) {
            is_first = true;
            start_time = os::Time::now();
            last_time = start_time;
            recv_file.open("./recv_file.mp4");
        }
        recv_size += buffer.data_size();
        if (os::Time::now() - last_time > 5000) {
            LOG_TRACE("recv_speed: %lf B/ms", (double)recv_size / (os::Time::now() - start_time));
            last_time = os::Time::now();
        }

        recv_file.write(buffer, buffer.data_size());
        // LOG_TRACE("Client request: %s", buffer.str().c_str());
        // return send_data(cid, buffer);
        return 0;
    }
private:
    os::File recv_file;
    bool is_first = false;
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