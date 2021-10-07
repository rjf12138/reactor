#ifndef __EPOLL_H__
#define __EPOLL_H__

#include "basic_head.h"
#include "reactor.h"
#include "util/util.h"

#ifdef __RJF_LINUX__
#include <sys/epoll.h>

namespace reactor {

class Epoll : public Event {
public:
    // timeout: 毫秒， events_size: 最大返回触发的事件数
    Epoll(bool main_handler = false, int timeout = 3000, int events_size = 32);
    virtual ~Epoll(void);

    virtual int event_ctl(EventHandle_t* handle) override;
    virtual int msg_handler(util::obj_id_t sender, const basic::ByteBuffer &msg);

    bool main_handler() const {return is_main_handler_;}

    // 事件处理函数和事件退出函数
    static void* event_wait(void *arg);
    static void* event_exit(void *arg);
    static void* event_send(void *arg); // 发送返回的消息

private:
    virtual struct epoll_event event_type_convt(uint32_t type);

private:
    bool exit_;         // 设置退出
    int epfd_;          // epoll fd
    int timeout_;       // epoll_wait 超时时间

    bool is_main_handler_; // 主事件处理器，true: 监听accept的fd, 等待客户端连接。false: 监听客户端连接，等待数据发送
    util::obj_id_t hander_;

    int events_max_size_;   // 一次最多返回的触发事件
    struct epoll_event *events_;
    
    std::map<int, EventHandle_t*> events_map_; // 事件信息
};


#endif
}

#endif