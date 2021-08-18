#ifndef __EPOLL_H__
#define __EPOLL_H__

#include "basic_head.h"
#include "event.h"

#ifdef __RJF_LINUX__
#include <sys/epoll.h>

namespace reactor {
class Epoll : public Event {
public:
    Epoll(int events_size = 32);
    virtual ~Epoll(void);

    virtual int event_init(int size = 5) override;
    virtual int event_ctl(event_handle_t fd, EventOperation op, events &event) override;
    // max_size: 可以忽略，events的大小在开始的时候就设好了
    virtual int event_wait(events_t *events, int max_size, int timeout) override;

private:
    virtual struct epoll_event event_type_convt(uint32_t type);

private:
    int epfd_;
    int events_size_;
    struct epoll_event *events_;

};


#endif
}

#endif