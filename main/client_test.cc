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
    client.connect("raw://127.0.0.1:12138");

    basic::ByteBuffer buffer(std::string("Hello, world!\n"));
    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        } else if (ch == 's') {
            for (int i = 0; i < 10000; ++i) {
                client.send_data(buffer);
                //os::Time::sleep(5);
            }
            LOG_GLOBAL_INFO("send over!");
        }
    }

    return 0;
}