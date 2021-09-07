#ifndef __REACTOR_H__
#define __REACTOR_H__

#include "basic/logger.h"
#include "data_structure/queue.h"
#include "basic/byte_buffer.h"
#include "system/system.h"

namespace reactor {
using namespace basic;

enum EventMethod {
    EventMethod_Unknown,
    EventMethod_Epoll,
};

enum EventType {
    EventType_In    = 0x001,    // 数据可读
    EventType_Pri   = 0x002,    // 高优先级数据可读，比如TCP带外数据
    EventType_Out   = 0x004,    // 数据可写
    EventType_RDHup = 0x008,    // 连接被对方关闭，或者对方关闭了写操作
    EventType_Err   = 0x010,    // 错误
    EventType_Hup   = 0x020,    // 挂起。比如管道写端关闭后，读端描述符上将收到POLLHUP事件
};

enum EventOperation {
    EventOperation_Add = 1, // 往事件上注册fd
    EventOperation_Mod = 2, // 修改事件上的fd
    EventOperation_Del = 3  // 删除事件上的fd
};

typedef void* (*handle_func_t)(void *arg);
typedef struct EventHandle {
    util::SocketTCP *tcp_conn;

    bool is_handling;       // 当前事件是不是已经有线程在处理了，防止两个线程处理同一个fd
    
    uint32_t type;          // EventType 与的集合
    EventOperation op;      // fd 上要进行的操作
    EventMethod method;     // 哪中类型的event, 目前只有epoll

    bool is_accept;         // 是不是acceptor的描述符

    // accept 的处理函数
    void *accept_arg;              // handle_func_t 参数
    handle_func_t accept_func;     // 当事件触发时的处理函数

    // 客户端连接时的处理函数
    void *client_arg;
    handle_func_t client_func;

    bool is_send_ready;         // buffer 中的数据是不是要发送的
    ByteBuffer recv_buffer;
    ByteBuffer send_buffer;
} EventHandle_t;

class Event : public Logger{
public:
    Event(void);
    virtual ~Event(void);

    virtual int event_init(int size = 5) = 0;
    virtual int event_ctl(EventHandle_t &handle) = 0;

    EventMethod get_type(void) const {return type_;}

private:
    EventMethod type_;
};

/*
 线程分配： reactor 有两个线程，一个是事件等待线程，一个是结果发送线程
 剩下的是工作线程
*/

typedef struct ReactorConfig {
    int min_work_threads_num;
    int max_work_threads_num;
} ReactorConfig_t;

class Reactor : public Logger {
public:
    virtual ~Reactor(void);

    static Reactor& get_instance(void);
    int set_config(ReactorConfig_t config);

    int event_init(void);
    int event_ctl(EventHandle_t &handle);

private:
    Reactor(void);
    Reactor(const Reactor&)=delete;
    Reactor& operator=(const Reactor&)=delete;

    static void* recv_buffer_func(void* arg);
    static void* reactor_exit(void* arg);

private:
    bool reactor_stop_;

    ReactorConfig_t config_;
    std::map<uint64_t, Event*> events_map_;

    util::ThreadPool thread_pool_;

    util::Mutex mutex_;
    ds::Queue<EventHandle_t*> recv_;
    ds::Queue<EventHandle_t*> send_;

    // 当事件已经有线程在处理，又有新的事件发生时
    // 防止多个线程处理同一个事件，先暂存在这
    std::map<int, EventHandle_t*> block_event_;
};

}

#endif