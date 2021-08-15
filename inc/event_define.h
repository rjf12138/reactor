#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

#include "basic_head.h"

enum EventType {
    EventType_In = 0,  // 数据可读
    EventType_Pri = 1, // 高优先级数据可读，比如TCP带外数据
    EventType_Out = 2, // 数据可写
    EventType_RDHup = 3, // 连接被对方关闭，或者对方关闭了写操作
    EventType_Err = 4, //错误
    EventType_Hup = 5, // 挂起。比如管道写端关闭后，读端描述符上将收到POLLHUP事件
    EventType_NVal = 6 // 文件描述符没有打开
};

enum EventOperation {
    EventOperation_Add = 1, // 往事件上注册fd
    EventOperation_Mod = 2, // 修改事件上的fd
    EventOperation_Del = 3  // 删除事件上的fd
};

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

class Events {
public:
    Events(void);
    virtual ~Events(void);

    virtual int event_init(int size = 5) = 0;
    virtual int event_ctl(int fd, EventOperation op, int type) = 0;
    virtual int event_wait(events_t *events, int max_size, int timeout) = 0;
private:
    int event_type_convt(EventType)
private:
    int fd_;
};

#endif