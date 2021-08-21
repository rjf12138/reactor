#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

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

typedef void (*handle_func_t)(int fd, void *arg);
typedef struct EventHandle {
    int fd;                 // fd 描述符
    bool is_handling;       // 当前事件是不是已经有线程在处理了，防止两个线程处理同一个fd
    uint32_t type;          // EventType 与的集合
    EventOperation op;      // fd 上要进行的操作

    bool is_accept;         // 是不是acceptor的描述符

    // accept 的处理函数
    void *accept_arg;              // handle_func_t 参数
    handle_func_t accept_func;     // 当事件触发时的处理函数

    // 客户端连接时的处理函数
    void *client_arg;
    handle_func_t client_func;

    ByteBuffer buffer;
} EventHandle_t;

class Event : public Logger{
public:
    Event(void);
    virtual ~Event(void);

    virtual int event_init(int size = 5) = 0;
    virtual int event_ctl(EventHandle_t &handle) = 0;

    EventMethod type(void) const {return type_;}

private:
    EventMethod type_;
};

class EventManager {
public:
    EventManager(EventMethod method);
    virtual ~EventManager(void);

    int init(void);
    int ctl(EventHandle_t &handle);

private:
    std::map<uint64_t, Event*> events_map_;

    util::ThreadPool thread_pool_;

    util::Mutex mutex_;
    ds::Queue<EventHandle_t*> recv_;
    ds::Queue<EventHandle_t*> send_;
};

}

#endif