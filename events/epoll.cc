#include "epoll.h"

namespace reactor {
#ifdef __RJF_LINUX__
Epoll::Epoll(int events_size = 32)
{
    events_size_ = 32;
    if (events_size > 0) {
        events_size_ = events_size;
    }
    events_ = new epoll_event[events_size_];
}

Epoll::~Epoll(void)
{
    if (events_ != nullptr) {
        delete events_;
        events_ = nullptr;
    }
}

int 
Epoll::event_init(int size = 5)
{
    epfd_ = epoll_create(size);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
        return -1;
    }

    return epfd_;
}

int 
Epoll::event_ctl(event_handle_t handle, EventOperation op, events &event)
{
    int epoll_op = 0;
    switch (op)
    {
    case EventOperation_Add: {
        epoll_op = EPOLL_CTL_ADD;
    } break;
    case EventOperation_Mod: {
        epoll_op = EPOLL_CTL_MOD;
    } break;
    case EventOperation_Del: {
        epoll_op = EPOLL_CTL_DEL;
    } break;
    default:
        break;
    }

    struct epoll_event events = this->event_type_convt(event.events);
    events.data.fd = handle.fd;

    int ret = epoll_ctl(epfd_, epoll_op, handle.fd, &events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    return ret;
}

int 
Epoll::event_wait(func, int timeout)
{
    int ret = epoll_wait(epfd_, events_, events_size_, timeout);
    if (ret == -1) {
        LOG_ERROR("epoll_wait: %s", strerror(errno));
        return -1;
    }

    func(events) {
        if ()
        If()
    }
}


#define CONVERT_TYPE(src, dst, x, y) \
{\
if (src & x) {\
    dst |= y;\
}\
}

struct epoll_event 
Epoll::event_type_convt(uint32_t type)
{
    struct epoll_event event = {0, 0};
    CONVERT_TYPE(type, event.events, EventType_In, EPOLLIN);
    CONVERT_TYPE(type, event.events, EventType_Pri, EPOLLPRI);
    CONVERT_TYPE(type, event.events, EventType_Out, EPOLLOUT);
    CONVERT_TYPE(type, event.events, EventType_RDHup, EPOLLRDHUP);
    CONVERT_TYPE(type, event.events, EventType_Err, EPOLLERR);
    CONVERT_TYPE(type, event.events, EventType_Hup, EPOLLHUP);

    return event;
}

#endif

}