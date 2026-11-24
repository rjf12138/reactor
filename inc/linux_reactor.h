#ifndef __SUB_REACTOR_H__
#define __SUB_REACTOR_H__

#include <atomic>
#include <mutex>

#include "reactor_define.h"

/********************************************************
* 线程数分配（至少5个线程）：
* 实时打印线程信息： 取决于 ReactorManager 是否开启
* SubReactor： 一个线程
* MainReactor: 一个线程
* SendDataCenter： 至少一个线程
* 处理客户端发送的数据： 至少一个线程
*********************************************************/

namespace reactor {
typedef enum ReactorState {
    ReactorState_Running,   // 运行
    ReactorState_WaitExit,  // 等待退出
    ReactorState_Exit,      // 退出
    ReactorState_Error,     // 异常
} ReactorState;

//////////////////////////// 处理客户端的数据 ////////////////////////////////////////////
class SubReactor : public Logger {
public:
    SubReactor(int events_max_size_ = 32);
    virtual ~SubReactor(void);

public:
    int start(void);
    int stop(void);

public:
    // 添加新的客户端连接
    int add_client_conn(ClientConn_t *client_conn_ptr);
    // 移除客户端连接
    int remove_client_conn(sock_id_t cid);
    // 优雅关闭：发送队列中的数据全部发完后关闭连接（用于 send_data 后立即 close_client）
    int close_client_gracefully(sock_id_t cid);
    // 发送数据（只入队，由事件线程异步发送，部分发送不会丢数据）
    ssize_t send_data(sock_id_t cid, ByteBuffer &buffer);
    // 获取客户端连接数量
    int get_client_conn_size(void);
    // 运行状态
    ReactorState get_state(void) {return reactor_state_.load();}

public:
    // 事件处理函数
    static void* event_wait(void *arg);

private:
    // 事件线程使用：在持锁状态下标记连接正在处理，返回连接指针
    ClientConn_t* acquire_client_conn(sock_id_t cid);
    // 事件线程使用：处理完成，必要时销毁被延迟删除的连接
    void release_client_conn(ClientConn_t *conn_ptr);
    // 调用者必须持有 mutex_：把 send_buffer 中的数据尽量发送出去
    ssize_t flush_send_buffer(ClientConn_t *conn_ptr);
    // 调用者必须持有 mutex_：更新 EPOLLOUT 注册状态
    int update_epoll_out(ClientConn_t *conn_ptr, bool enable);
    // 连接断开通知（不持锁调用）
    void notify_disconnected(ClientConn_t *conn_ptr);

    SubReactor(const SubReactor &) = delete;
    SubReactor& operator=(const SubReactor&) = delete;

private:
    int epfd_;
    int events_max_size_;   // 一次最多返回的触发事件
    struct epoll_event *events_;

    std::mutex mutex_;

    std::atomic<ReactorState> reactor_state_;
    std::map<sock_id_t, ClientConn_t*> client_conn_; // 客户端连接
};

/////////////////////////// 处理客户端的连接 ///////////////////////////////////////////
class MainReactor : public Logger {
public:
    MainReactor(int sub_reactor_size = 5, int main_events_max_size = 32, int sub_events_max_size = 32);
    virtual ~MainReactor(void);

public:
    // 启动
    int start(void);
    // 停止
    int stop(void);

public:
    // 获取运行状态
    ReactorState get_state(void) {return reactor_state_.load();}
    // 添加服务端监听连接
    int add_server_accept(EventHandle_t *handle_ptr);
    // 删除服务端监听连接
    int remove_server_accept(sock_id_t sid);
    // 移除客户端连接
    int remove_client_conn(sock_id_t cid);
    // 将连接分配到sub reactor上
    SubReactor * assign_conn_to_sub_reactor(ClientConn_t *conn);
    // 获取客户端连接所在的sub reactor
    SubReactor * get_client_sub_reactor(sock_id_t cid);

public: 
    // 事件处理函数
    static void* event_wait(void *arg);
    // 打印连接信息
    static void* show_connect_info(void *arg);

private:
    MainReactor(const MainReactor &) = delete;
    MainReactor& operator=(const MainReactor&) = delete;

    // 关闭监听、清理连接和 sub reactor
    void cleanup(void);
    // SubReactor 移除连接时清理连接关系表
    static void on_client_closed(sock_id_t cid, void *arg);

private:
    int epfd_;
    int main_events_max_size_;   // 一次最多返回的触发事件
    int sub_events_max_size_;    // 一次最多返回的触发事件
    struct epoll_event *events_;

    int sub_reactor_size_;

    std::atomic<ReactorState> reactor_state_;
    std::mutex mutex_; // 保护下面的连接关系表

    std::map<sock_id_t, EventHandle_t*> acceptor_;  // 服务端句柄
    std::set<SubReactor*> set_sub_reactors_;        // subreactor集合

    std::map<sock_id_t, std::set<sock_id_t>> mp_srv_clients_;   // 服务端所对应的客户端连接
    std::map<sock_id_t, SubReactor*> mp_cli_to_sub_reactor_;    // 客户端所对应的Sub reactor

};


//////////////////////////// Reactor Manager //////////////////////////////////////////////////
class ReactorManager {
public:
    static ReactorManager& instance(void);
    virtual ~ReactorManager(void);

    // 启动Reactor。成功返回0，失败返回-1
    int start(const ReactorConfig_t &config);
    // 停止Reactor
    int stop(void);

    // 获取reactor运行状态
    MainReactor* get_reactor(void) {return main_reactor_ptr;}

    // 设置线程池配置
    int set_config(const ReactorConfig_t &config);
    // 添加线程任务。成功返回大于0，失败返回0
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

    std::mutex mutex_;
    MainReactor* main_reactor_ptr;
};
}
#endif