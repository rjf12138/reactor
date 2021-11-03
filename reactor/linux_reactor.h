#ifndef __SUB_REACTOR_H__
#define __SUB_REACTOR_H__

#include "reactor.h"

namespace reactor {

class MsgHandleCenter {
public:
    static MsgHandleCenter& instance(void);
    virtual ~MsgHandleCenter(void);

    int set_config(const ReactorConfig_t &config);
    int add_task(os::Task &task);

private:
    MsgHandleCenter(void);
    MsgHandleCenter(const MsgHandleCenter&) = delete;
    MsgHandleCenter& operator=(const MsgHandleCenter&) = delete;

private:
    ReactorConfig_t config_;
    os::ThreadPool thread_pool_;
};

///////////////////////////// 发送数据处理 ////////////////////////////////////////////
// TODO: 如何为 senddatacenter 分配线程， 以及添加写注册
class SendDataCenter {
public:
    static SendDataCenter& instance(void);
    virtual ~SendDataCenter(void);

    int send_data(client_id_t cid);
    int register_connection(ClientConn_t *client_ptr);
    int remove_connection(client_id_t cid);

private:
    SendDataCenter(void);
    SendDataCenter(const SendDataCenter&) = delete;
    SendDataCenter& operator=(const SendDataCenter&) = delete;

    static void* send_loop(void* arg);
    static void* exit_loop(void* arg);

private:
    static SendDataCenter *send_center_;
    
    bool send_exit_;

    os::Mutex send_mtx_;
    ds::Queue<client_id_t> send_queue_;

    os::Mutex conn_mtx_;
    std::map<client_id_t, ClientConn_t*> sender_conns_;
};

//////////////////////////// 处理客户端的数据 ////////////////////////////////////////////
class SubReactor : public Logger {
public:
    static SubReactor& instance(void);
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
    // 发送返回的消息
    static void* event_send(void *arg); 

private:
    inline EventHandle_t* get_event_handle(int client_sock);

    SubReactor(int events_max_size_ = 32, int timeout = 3000);
    SubReactor(const SubReactor &) = delete;
    SubReactor& operator=(const SubReactor&) = delete;

private:
    bool exit_;
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
    static MainReactor& instance(void);
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
    // 发送返回的消息
    static void* event_send(void *arg); 

private:
    MainReactor(int events_max_size_ = 32, int timeout = 3000);
    MainReactor(const MainReactor &) = delete;
    MainReactor& operator=(const MainReactor&) = delete;

    inline EventHandle_t* get_event_handle(int listen_socket_fd);
private:
    bool exit_;
    int epfd_;

    int timeout_;
    int events_max_size_;   // 一次最多返回的触发事件
    struct epoll_event *events_;

    os::Mutex server_ctl_mutex_;
    std::map<int, server_id_t> server_listen_to_;
    std::map<server_id_t, EventHandle_t*> acceptor_;
};


}
#endif