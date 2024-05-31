/*
    测试客户端往服务器发送数据
*/
#include "reactor.h"

using namespace reactor;

class TestClient : public HttpNetClient {
public:
    TestClient(int *total_send_count, int* send_count, int *success_count, int* error_count)
        : success_count_ptr(success_count),
        error_count_ptr(error_count),
        send_count_ptr(send_count),
        total_send_count_ptr(total_send_count)
    {
        ptl.set_request(HTTP_METHOD_GET, "/");
        current_send_count = 1;

        this->request_buffer.clear();
        for (int j = 0; j < 1000000; ++j) {
            this->request_buffer.write_int8(rand() % 256);
        }
    }
    ~TestClient(void) {}

    int handle_msg(ptl::HttpPtl &http_ptl, ptl::HttpParse_ErrorCode err) {
        // if (http_ptl.get_header_option(HTTP_HEADER_Host) != get_ip_info()) {
        //     LOG_GLOBAL_WARN("Recv msg[ptl Host: %s, TestClient Host: %s]", http_ptl.get_header_option(HTTP_HEADER_Host).c_str(), get_ip_info().c_str());
        //     return 0;
        // }

        if (http_ptl.get_status_code() != HTTP_STATUS_OK) {
            LOG_GLOBAL_WARN("Recv msg[ptl url: %d, TestClient Status code: 200]", http_ptl.get_status_code());
            return 0;
        }

        std::string ret_time = http_ptl.get_header_option("Time");
        if (http_ptl.get_content() != request_buffer || ret_time != send_time) {
            LOG_GLOBAL_WARN("Recv msg[ptl content: %s, TestClient content: %s]", http_ptl.get_content().str().c_str(), request_buffer.data_size() > 0 ? request_buffer.str().c_str() : "");
            ++(*error_count_ptr);
        } else {
            LOG_GLOBAL_INFO("Successfully receive server response[%ld]", os::Time::now());
            ++(*success_count_ptr);
        }
        this->disconnect();

        if (current_send_count < *total_send_count_ptr) {
            while (this->get_state() == NetConnectState_Connected) {
                ;
            }
            this->connect("http://127.0.0.1:12138"); 
            this->send_msg();
            ++current_send_count;
        }
        return 0;
    }

    int notify_client_disconnected(sock_id_t cid) {
        LOG_TRACE("client disconnected[cid: %d]", cid);
        return 0;
    }

    void send_msg(void) {
        send_time = os::Time::format();
        ptl.set_header_option("Time", send_time);
        ptl.set_content(this->request_buffer);
        this->send_data(this->ptl);

        return ;
    }
private:
    int *send_count_ptr;
    int *total_send_count_ptr;
    int *success_count_ptr;
    int *error_count_ptr;
    int current_send_count;
    ptl::HttpPtl ptl;
    ByteBuffer request_buffer;
    ByteBuffer response_buffer;
    std::string send_time;
};

int main(int argc, char **argv)
{
    ReactorConfig_t rconfig;
    rconfig.sub_reactor_size_ = 200;
    rconfig.main_reactor_max_events_size_ = 200;
    rconfig.sub_reactor_max_events_size_ = 200;
    reactor_start(rconfig);

    int send_msg_count = 10;
    int client_count = 100;

    int send_count = 0;
    int success_count = 0;
    int error_count = 0;
    std::set<TestClient*> set_clients;
    os::mtime_t start_time = os::Time::now();
    for (int i = 0; i < client_count; ++i) {
        TestClient *client_ptr = new TestClient(&send_msg_count, &send_count, &success_count, &error_count);
        set_clients.insert(client_ptr);
    }
    os::mtime_t end_time = os::Time::now();
    LOG_GLOBAL_INFO("spend_time: %lld", end_time - start_time);

    int start_port = 12138;
    int server_max_count = 100;

    for (auto iter = set_clients.begin(); iter != set_clients.end(); ++iter) {
        while ((*iter)->get_state() == NetConnectState_Connected) {
            (*iter)->disconnect();
        }

        if ((*iter)->get_state() == NetConnectState_Disconnected) {
            char buffer[256] = {0};
            snprintf(buffer, 255, "http://127.0.0.1:%d", start_port + rand() % 100);
            (*iter)->connect(buffer);
        }
        (*iter)->send_msg();
    }

    fprintf(stdout, "Press 'q' to stop and show result\n"
    );

    while (true) {
        char ch = getchar();
        if (ch == 'q') {
            break;
        }
    }

    fprintf(stdout, "total_count: %d, send_count: %d, success_recv: %d, failed_recv: %d\n",
        send_msg_count * client_count,
        send_count,
        success_count,
        error_count
    );

    reactor_stop();
    return 0;
}