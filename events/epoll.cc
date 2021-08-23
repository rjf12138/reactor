#include "epoll.h"

namespace reactor {
#ifdef __RJF_LINUX__
Epoll::Epoll(util::Mutex *mutex, ds::Queue<EventHandle_t*> *recv, int timeout, int events_size)
:   exit_(false),
    timeout_(timeout),
    events_max_size_(events_size)
{
    recv_queue_mtx_ = mutex;
    recv_ = recv;

    if (events_size > 0) {
        events_max_size_ = events_size;
    }
    events_ = new epoll_event[events_max_size_];
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
Epoll::event_ctl(EventHandle_t &handle)
{
    if (handle.tcp_conn.get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error socket state: %d", handle.tcp_conn.get_socket());
        return -1;
    }

    int epoll_op = 0;
    switch (handle.op)
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
    default:{
        LOG_ERROR("epoll_ctl: Unknown EventOperate: %d", handle.op);
        return -1;
    }
    }

    struct epoll_event events = this->event_type_convt(handle.type);
    events.data.fd = handle.tcp_conn.get_socket();

    int ret = epoll_ctl(epfd_, epoll_op, handle.tcp_conn.get_socket(), &events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    if (handle.op == EventOperation_Del) {
        auto del_iter = events_map_.find(handle.tcp_conn.get_socket());
        events_map_.erase(del_iter);
    } else {
        events_map_[handle.tcp_conn.get_socket()] = handle;
        events_map_[handle.tcp_conn.get_socket()].is_send_ready = false; // 当前缓存置为false， 不发送
    }
    return ret;
}

void*
Epoll::event_wait(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    Epoll *epoll_ptr = (Epoll*)arg;
    while (epoll_ptr->exit_ == false) {
        int ret = ::epoll_wait(epoll_ptr->epfd_, epoll_ptr->events_, epoll_ptr->events_max_size_, epoll_ptr->timeout_);
        if (ret == -1) {
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            return nullptr;
        } else if (ret == 0) {
            continue;
        }

        epoll_ptr->recv_queue_mtx_->lock();
        for (int i = 0; i < ret; ++i) {
            auto find_iter = epoll_ptr->events_map_.find(epoll_ptr->events_[i].data.fd);
            if (find_iter == epoll_ptr->events_map_.end()) {
                continue;
            }

            epoll_ptr->recv_->push(&find_iter->second);
            find_iter->second.is_send_ready = false; // 当前缓存置为false， 不发送
        }
        epoll_ptr->recv_queue_mtx_->unlock();
    }

    return nullptr;
}

void* 
Epoll::event_send(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    Epoll *epoll_ptr = (Epoll*)arg;
    while (epoll_ptr->exit_ == false) {
        for (auto iter = epoll_ptr->events_map_.begin(); iter != epoll_ptr->events_map_.end(); ++iter) {
            if (iter->second.is_send_ready == true) {
                iter->second.tcp_conn.send(iter->second.buffer, iter->second.buffer.data_size(), 0);
                iter->second.is_send_ready = false;
            }
        }
        util::os_sleep(20);
    }

    return nullptr;
}

void* 
Epoll::event_exit(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    Epoll *epoll_ptr = (Epoll*)arg;
    epoll_ptr->exit_ = true;

    return nullptr;
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