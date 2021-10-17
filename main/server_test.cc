#include "reactor.h"

using namespace reactor;

class TestServer : public NetServer {
public:
    TestServer(void) {}
    ~TestServer(void) {}

    virtual int handle_msg(client_id_t cid, basic::ByteBuffer &buffer) {
        LOG_TRACE("Client request: %s", buffer.str().c_str());
        return send_data(cid, buffer);
    }
};

int main(int argc, char **argv)
{
    TestServer server;
    server.start("127.0.0.1", 12138, ptl::ProtocolType_Raw);

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        }
    }


    return 0;
}