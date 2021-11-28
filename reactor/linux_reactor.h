#ifndef __SUB_REACTOR_H__
#define __SUB_REACTOR_H__

#include "reactor.h"

/********************************************************
* 线程数分配（至少5个线程）：
* 实时打印线程信息： 取决于 ReactorManager 是否开启
* SubReactor： 一个线程
* MainReactor: 一个线程
* SendDataCenter： 至少一个线程
* 处理客户端发送的数据： 至少一个线程
*********************************************************/

namespace reactor {
enum ReactorState {
    ReactorState_Running,
    ReactorState_WaitExit,
    ReactorState_Exit
};

///////////////////////////// 发送数据处理 ////////////////////////////////////////////
class SendDataCenter {
public:
    SendDataCenter(void);
    virtual ~SendDataCenter(void);

    // 向对端发送数据
    int send_data(client_id_t cid);
    // 注册发送对象
    int register_connection(ClientConn_t *client_ptr);
    // 移除发送对象
    int remove_connection(client_id_t cid);

private:
    SendDataCenter(const SendDataCenter&) = delete;
    SendDataCenter& operator=(const SendDataCenter&) = delete;

    static void* send_loop(void* arg);
    static void* exit_loop(void* arg);

private:
    os::Mutex send_mtx_;
    ds::Queue<client_id_t> send_queue_;

    os::Mutex conn_mtx_;
    std::map<client_id_t, ClientConn_t*> sender_conns_;
};

//////////////////////////// 处理客户端的数据 ////////////////////////////////////////////
class SubReactor : public Logger {
public:
    SubReactor(int events_max_size_ = 32, int timeout = 3000);
    virtual ~SubReactor(void);

    // 注册服务
    int server_register(EventHandle_t *handle_ptr);
    // 添加新的客户端连接
    int add_client_conn(server_id_t id, ClientConn_t *client_conn_ptr);
    // 移除客户端连接
    int remove_client_conn(server_id_t sid, client_id_t cid);

    // 事件处理函数
    static void* event_wait(void *arg);
    // 事件退出函数
    static void* event_exit(void *arg);

private:
    inline EventHandle_t* get_event_handle(int client_sock);

    SubReactor(const SubReactor &) = delete;
    SubReactor& operator=(const SubReactor&) = delete;

private:
    int epfd_;
    int timeout_;
    int events_max_size_;   // 一次最多返回的触发事件
    struct epoll_event *events_;

    std::map<int, server_id_t> client_conn_to_;
    std::map<server_id_t, EventHandle_t*> servers_;
};

/////////////////////////// 处理客户端的连接 ///////////////////////////////////////////
class MainReactor : public Logger {
public:
    MainReactor(int events_max_size_ = 32, int timeout = 3000);
    virtual ~MainReactor(void);

    // 添加服务端监听连接
    int add_server_accept(EventHandle_t *handle_ptr);
    // 删除服务端监听连接
    int remove_server_accept(server_id_t sid);
    // 移除客户端连接
    int remove_client_conn(server_id_t sid, client_id_t cid);

    // 事件处理函数
    static void* event_wait(void *arg);
    // 事件退出函数
    static void* event_exit(void *arg);

private:
    MainReactor(const MainReactor &) = delete;
    MainReactor& operator=(const MainReactor&) = delete;

    inline EventHandle_t* get_event_handle(int listen_socket_fd);
private:
    int epfd_;
    int timeout_;
    int events_max_size_;   // 一次最多返回的触发事件
    struct epoll_event *events_;

    os::Mutex server_ctl_mutex_;
    std::map<int, server_id_t> server_listen_to_;
    std::map<server_id_t, EventHandle_t*> acceptor_;
};


//////////////////////////// Reactor Manager //////////////////////////////////////////////////
class ReactorManager {
public:
    static ReactorManager& instance(void);
    virtual ~ReactorManager(void);

    // 启动Reactor
    int start(const ReactorConfig_t &config);
    // 停止Reactor
    int stop(void);

    MainReactor* get_main_reactor(void) {return main_reactor_ptr;}
    SubReactor* get_sub_reactor(void) {return sub_reactor_ptr;}
    SendDataCenter *get_send_datacenter(void) {return send_datacenter_ptr;}

    void set_main_reactor_state(ReactorState state) {main_reactor_state_ = state;}
    void set_sub_reactor_state(ReactorState state) {sub_reactor_state_ = state;}
    void set_send_datacenter_state(ReactorState state) {send_datacenter_state_ = state;}

    ReactorState get_main_reactor_state(void) {return main_reactor_state_;}
    ReactorState get_sub_reactor_state(void) {return sub_reactor_state_;}
    ReactorState get_send_datacenter_state(void) {return send_datacenter_state_;}

    // 设置线程池配置
    int set_config(const ReactorConfig_t &config);
    // 添加线程任务
    int add_task(os::Task &task);

    // 添加定时任务
    int add_timer(util::TimerEvent_t event);
    // 取消定时任务
    int cancel_timer(int timer_id);

private:
    ReactorManager(void);
    ReactorManager(const ReactorManager&) = delete;
    ReactorManager& operator=(const ReactorManager&) = delete;

private:
    ReactorConfig_t config_;

    os::ThreadPool thread_pool_;
    util::Timer timer_;

    MainReactor* main_reactor_ptr;
    SubReactor* sub_reactor_ptr;
    SendDataCenter *send_datacenter_ptr;

    ReactorState main_reactor_state_;
    ReactorState sub_reactor_state_;
    ReactorState send_datacenter_state_;
};
}
#endif