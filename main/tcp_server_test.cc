#include "reactor.h"

using namespace reactor;

class TestServer : public NetServer {
public:
    TestServer(void) {
        ptl_.set_response(HTTP_STATUS_OK, "OK");
    }

    ~TestServer(void) 
    {}

    virtual int handle_msg(sock_id_t cid, ptl::HttpPtl &http_ptl, ptl::HttpParse_ErrorCode err) {
        ptl_.set_content(http_ptl.get_content());
        ptl_.set_header_option("Time", http_ptl.get_header_option("Time"));

        ByteBuffer buffer;
        ptl_.generate(buffer);

        LOG_GLOBAL_INFO("Response: %s", buffer.str().c_str());
        this->send_data(cid, buffer);

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

private:
    uint64_t recv_size = 0;
    ptl::HttpPtl ptl_;
};

int main(int argc, char **argv)
{
    ReactorConfig_t rconfig;
    rconfig.sub_reactor_size_ = 14;
    rconfig.main_reactor_max_events_size_ = 2000;
    rconfig.sub_reactor_max_events_size_ = 10;
    reactor_start(rconfig);

    int start_port = 12138;
    int server_count = 100;

    std::set<TestServer*> set_servers;
    for (int i = start_port; i <= start_port + server_count; ++i) {
        TestServer *server_ptr = new TestServer();
        set_servers.insert(server_ptr);

        server_ptr->start("127.0.0.1", i, ptl::ProtocolType_Http); 
    }

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        }
    }

    reactor_stop();
    return 0;
}