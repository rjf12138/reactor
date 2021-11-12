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
};

int main(int argc, char **argv)
{
    TestClient client;
    client.connect("raw://192.168.0.103:12138");

    basic::ByteBuffer buffer(std::string("Hello, world!\n"));
    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        } else if (ch == 's') {
            os::File send_file;
            send_file.open("/media/ruanjian/data/adv_immu/[44x.me]SSPD-144.mp4");
            buffer.clear();
            while(true) {
                int read_size = send_file.read(buffer, 2000);
                if (read_size <= 0) {
                    break;
                }
                client.send_data(buffer);
                os::Time::sleep(100);
            }
            // for (int i = 0; i < 10000; ++i) {
            //     client.send_data(buffer);
            //     os::Time::sleep(2);
            // }
            LOG_GLOBAL_INFO("send over!");
        }
    }

    return 0;
}