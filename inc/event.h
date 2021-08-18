#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

#include "logger.h"

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

typedef union event_handle {
    int fd;
} event_handle_t;

typedef union event_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} event_data_t;

typedef struct events {
    event_data_t data;
    uint32_t events;
} events_t;

class Event : public Logger{
public:
    Event(void);
    virtual ~Event(void);

    virtual int event_init(int size = 5) = 0;
    virtual int event_ctl(event_handle_t handle, EventOperation op, events &event) = 0;
    virtual int event_wait(events_t *events, int max_size, int timeout) = 0;
};

class EventManager {
public:
    EventManager(EventMethod method);
    virtual ~EventManager(void);

    int event_init(int size = 5);
    int event_ctl(event_handle_t handle, EventOperation op, events &event);
    int event_wait(events_t *events, int max_size, int timeout);

private:
    EventMethod method_;
    Event *event_;
};

}

#endif