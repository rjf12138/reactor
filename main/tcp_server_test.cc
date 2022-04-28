#include "reactor.h"

using namespace reactor;

class TestServer : public NetServer {
public:
    TestServer(void) {
        request_buffer.write_string("Request: Hello, world!!!!");
        response_buffer.write_string("Response: Hello, world!!!!");

        ptl_.set_response(HTTP_STATUS_OK, "OK");
        // ptl_.set_header_option(HTTP_HEADER_Host, get_ip_info());
        ptl_.set_content(response_buffer);
    }

    ~TestServer(void) 
    {}

    virtual int handle_msg(client_id_t cid, ptl::HttpPtl &http_ptl) {
        ByteBuffer buffer;
        ptl_.generate(buffer);
        os::Time time_x;
        ptl_.set_header_option("ResponseTime", time_x.format());
        ptl_.set_header_option("SendStartTime", http_ptl.get_header_option("SendStartTime"));
        this->send_data(cid, buffer);

        return 0;
    }

    int notify_client_disconnected(client_id_t cid) {
        LOG_TRACE("server client disconnected[cid: %d]", cid);
        return 0;
    }

    int notify_server_stop_listen(void) {
        LOG_TRACE("server stop listen!");
        return 0;
    }

private:
    uint64_t recv_size = 0;
    ptl::HttpPtl ptl_;
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

    TestServer server;
    server.start("192.168.0.103", 12138, ptl::ProtocolType_Http);

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        }
    }

    reactor_stop();
    return 0;
}