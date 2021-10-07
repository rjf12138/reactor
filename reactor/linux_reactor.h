#ifndef __SUB_REACTOR_H__
#define __SUB_REACTOR_H__

#include "reactor.h"

namespace reactor {

class MsgHandleCenter {
public:
    static MsgHandleCenter* instance(void);
    static void destory(void);

    virtual ~MsgHandleCenter(void);

    int set_config(const ReactorConfig_t &config);
    int add_task(os::Task &task);

    int push_recv_handle(EventHandle_t* handle_ptr);

private:
// TODO: 如何设置消息线程的数量
    MsgHandleCenter(void) {
        os::ThreadPoolConfig threadpool_config = thread_pool_.get_threadpool_config();
        threadpool_config.min_thread_num = 6;  // 1个main_reactor, 1个sub_reactor, 4个消息处理线程
        threadpool_config.max_thread_num = 18; // 1个main_reactor, 1个sub_reactor, 16个消息处理线程
    }
    
private:
    static MsgHandleCenter *msg_handle_center_;

    ReactorConfig_t config_;
    os::ThreadPool thread_pool_;

    os::Mutex recv_mutex_;
    ds::Queue<EventHandle_t*> recv_;
    os::Mutex send_mutex_;
    ds::Queue<EventHandle_t*> send_;
};

class SubReactor : public Logger{
public:
    SubReactor(int events_max_size_ = 32, int timeout = 3000);
    virtual ~SubReactor(void);

    // 添加/修改/删除客户端连接
    int client_ctl(EventHandle_t *handle_ptr);

    // 事件处理函数
    static void* event_wait(void *arg);
    // 事件退出函数
    static void* event_exit(void *arg);
    // 发送返回的消息
    static void* event_send(void *arg); 

private:
    bool exit_;
    int epfd_;

    int timeout_;
    int events_max_size_;   // 一次最多返回的触发事件
    struct epoll_event *events_;

    std::map<int, EventHandle_t*> client_;
};

}
#endif