#ifndef __REACTOR_H__
#define __REACTOR_H__

#include "basic/logger.h"
#include "data_structure/queue.h"
#include "basic/byte_buffer.h"
#include "system/system.h"
#include "util/util.h"

namespace reactor {
using namespace basic;
/* 消息格式
{
    "msg_id": id[INT], //消息ID
    "socket": socket[INT], //socket描述符
    "server_handler": id[INT], // 上层的服务类ID，用于调用数据处理函数
    "recv_buffer": buffer[ByteBuffer*]，// 接收缓存
    "send_buffer": buffer[ByteBuffer*]，// 发送缓存
    "event_handler_ptr": ptr[EventHandle_t*] 句柄指针
}
*/
#define EVENT_MSG_NAME_MSGID                "msg_id"
#define EVENT_MSG_NAME_SOCKET               "socket"
#define EVENT_MSG_NAME_SERVER_HANDLE        "server_handler"
#define EVENT_MSG_NAME_RECV_BUFFER          "recv_buffer"
#define EVENT_MSG_NAME_SEND_BUFFER          "send_buffer"
#define EVENT_MSG_NAME_EVENT_HANDLER_PTR    "event_handler_ptr"

enum EventMsgId {
    EventMsgId_AddHandle = 100,           // 添加accept句柄，监听客户端的连接
    EventMsgId_RecvClientDataEvent = 101,       // 客户端收到数据的事件
};

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

enum EventHandleState {  // 事件当前状态
    EventHandleState_Idle,      // 未触发任何事件
    EventHandleState_Ready,     // 事件已经触发但是还未处理
    EventHandleState_Handling,  // 正在处理就绪事件
};
typedef void* (*handle_func_t)(void *arg);
typedef struct EventHandle {
    os::SocketTCP *acceptor;    // 监听套接字连接

    os::Mutex client_conn_mutex;
    ds::Queue<os::SocketTCP*> client_conn; // 客户端连接

    EventHandleState state;     // 事件当前状态

    uint32_t events;            // 事件集合

    EventOperation op;          // fd 上要进行的操作
    EventMethod method;         // 哪中类型的event, 目前只有epoll

    // 客户端连接时的处理函数
    void *client_arg;
    handle_func_t client_func;

    bool is_send_ready;         // buffer 中的数据是不是要发送的
    ByteBuffer recv_buffer;
    ByteBuffer send_buffer;
} EventHandle_t;

class Event : public Logger, public util::MsgObject {
public:
    Event(void);
    virtual ~Event(void);

    virtual int event_init(int size = 5) = 0;
    virtual int event_ctl(EventHandle_t *handle) = 0;

    EventMethod get_type(void) const {return type_;}
    void set_main_handler(bool is_main) {is_main_handler_ = is_main;} 
    
private:
    EventMethod type_;
    bool is_main_handler_; // reactor 中会有两个Event。一个是主的处理客户端连接，一个是辅的处理客户端数据
};

/*
 线程分配： reactor 有两个线程，一个是事件等待线程，一个是结果发送线程
 剩下的是工作线程
*/

typedef struct ReactorConfig {
    uint32_t min_thread_num; // 最小线程数
    uint32_t max_thread_num; // 最大线程数
} ReactorConfig_t;

class Reactor : public Logger, public util::MsgObject {
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

    os::ThreadPool thread_pool_;

    os::Mutex mutex_;
    ds::Queue<EventHandle_t*> recv_;
    ds::Queue<EventHandle_t*> send_;

    // 当事件已经有线程在处理，又有新的事件发生时
    // 防止多个线程处理同一个事件，先暂存在这
    std::map<int, EventHandle_t*> event_buffer_;
};

}

#endif