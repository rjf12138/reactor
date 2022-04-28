#include "reactor.h"

using namespace reactor;

class TestClient : public HttpNetClient {
public:
    TestClient(void) 
    {
        request_buffer.write_string("Request: Hello, world!!!!");
        response_buffer.write_string("Response: Hello, world!!!!");

        ptl.set_request(HTTP_METHOD_GET, "/Request"); // 服务端返回是 /Response
        // ptl.set_header_option(HTTP_HEADER_Host, get_ip_info());
        ptl.set_content(request_buffer);
    }
    ~TestClient(void) {}

    int handle_msg(ptl::HttpPtl &http_ptl) {
        // if (http_ptl.get_header_option(HTTP_HEADER_Host) != get_ip_info()) {
        //     LOG_GLOBAL_WARN("Recv msg[ptl Host: %s, TestClient Host: %s]", http_ptl.get_header_option(HTTP_HEADER_Host).c_str(), get_ip_info().c_str());
        //     return 0;
        // }

        if (http_ptl.get_status_code() != HTTP_STATUS_OK) {
            LOG_GLOBAL_WARN("Recv msg[ptl url: %d, TestClient Status code: 200]", http_ptl.get_status_code());
            return 0;
        }

        if (http_ptl.get_content() != response_buffer) {
            LOG_GLOBAL_WARN("Recv msg[ptl content: %s, TestClient content: %s]", http_ptl.get_content().str().c_str(), response_buffer.str().c_str());
            return 0;
        }
        LOG_GLOBAL_INFO("Successfully receive server response[%ld]", os::Time::now());

        return 0;
    }

    int notify_client_disconnected(client_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }

    static void* send_timer_task(void *arg) {
        if (arg == nullptr) {
            LOG_GLOBAL_WARN("arg is nullptr");
            return nullptr;
        }

        TestClient* client_ptr = static_cast<TestClient*>(arg);
        os::Time time_x;
        client_ptr->ptl.set_header_option("SendStartTime", time_x.format());
        client_ptr->send_data(client_ptr->ptl);

        return nullptr;
    }
private:
    ptl::HttpPtl ptl;
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

    TestClient client;
    client.connect("http://192.168.0.103:12138");

    uint32_t send_gap = 500; // 单位：ms
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
            util::TimerEvent_t event;
            event.attr = util::TimerEventAttr_ReAdd;
            event.wait_time = send_gap;
            event.TimeEvent_callback = TestClient::send_timer_task;
            event.TimeEvent_arg = &client;

            client.add_timer_task(event);
        }
    }

    reactor_stop();
    return 0;
}