#include "reactor.h"

using namespace reactor;

class TestServer : public NetServer {
public:
    TestServer(void) {
    }

    ~TestServer(void) 
    {}

    virtual int handle_msg(sock_id_t cid, ptl::HttpPtl &http_ptl, ptl::HttpParse_ErrorCode err) {
        ByteBuffer buffer;
        // os::Time time_x;
        // http_ptl.set_header_option("ResponseTime", time_x.format());
        // http_ptl.set_header_option("SendStartTime", http_ptl.get_header_option("SendStartTime"));
        // http_ptl.generate(buffer);
        ptl::HttpPtl ptl_;
        ptl_.set_response(HTTP_STATUS_OK, "OK");

        ByteBuffer data;
        data.write_string("Hello, world!");
        ptl_.set_content(data);


        ptl_.generate(buffer);
        this->send_data(cid, buffer);
        LOG_GLOBAL_INFO("Respone:\n%s", buffer.str().c_str());
        this->close_client(cid);
        return 0;
    }

    int notify_client_disconnected(sock_id_t cid) {
        LOG_TRACE("server client disconnected[cid: %d]", cid);
        return 0;
    }

    int notify_server_stop_listen(void) {
        LOG_TRACE("server stop listen!");
        return 0;
    }
};

int main(int argc, char **argv)
{
    ReactorConfig_t rconfig;
    rconfig.sub_reactor_size_ = 5;
    reactor_start(rconfig);

    TestServer server;
    server.start("127.0.0.1", 12138, ptl::ProtocolType_Http);

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        }
    }

    reactor_stop();
    return 0;
}