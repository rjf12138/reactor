#include "linux_reactor.h"

namespace reactor {

#define CONVERT_TYPE(src, dst, x, y) \
{\
if (src & x) {\
    dst |= y;\
}\
}

struct epoll_event event_type_convt(uint32_t type)
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
//////////////////////////////////////////////////////////////////////////////////////////

MsgHandleCenter *MsgHandleCenter::msg_handle_center_ = nullptr;
MsgHandleCenter* 
MsgHandleCenter::instance(void)
{
    if (msg_handle_center_ == nullptr) {
        msg_handle_center_ = new MsgHandleCenter();
    }

    return msg_handle_center_;
}

void 
MsgHandleCenter::destory(void)
{
    if (msg_handle_center_ != nullptr) {
        msg_handle_center_->thread_pool_.stop_handler();
        delete msg_handle_center_;
        msg_handle_center_ = nullptr;
    }
}

MsgHandleCenter::~MsgHandleCenter(void)
{

}

int 
MsgHandleCenter::set_config(const ReactorConfig_t &config)
{
    os::ThreadPoolConfig threadpool_config = thread_pool_.get_threadpool_config();
    threadpool_config.max_thread_num = config.max_work_threads_num;
    threadpool_config.min_thread_num = config.min_work_threads_num;
    thread_pool_.set_threadpool_config(threadpool_config);

    return 0;
}

int 
MsgHandleCenter::add_task(os::Task &task)
{
    return thread_pool_.add_task(task);
}

int 
MsgHandleCenter::push_recv_handle(EventHandle_t* handle_ptr)
{
    recv_mutex_.lock();
    recv_.push(handle_ptr);
    recv_mutex_.unlock();

    return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////
SubReactor::SubReactor(int events_max_size, int timeout)
: events_max_size_(events_max_size),
  timeout_(timeout),
  exit_(false)
{
    if (events_max_size_ < 0) {
        events_max_size_ = 32;
    }
    events_ = new epoll_event[events_max_size_];

    epfd_ = epoll_create(5);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
    }
}

SubReactor::~SubReactor(void)
{
    if (events_ != nullptr) {
        delete events_;
        events_ = nullptr;
    }
}

int 
SubReactor::client_ctl(EventHandle_t *handle_ptr)
{
    if (handle_ptr == nullptr) {
        LOG_ERROR("handle_ptr is nullptr");
        return -1;
    }

    if (handle_ptr->tcp_conn->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error socket state: %d", handle_ptr->tcp_conn->get_socket());
        return -1;
    }

    int epoll_op = 0;
    switch (handle_ptr->op)
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
            LOG_ERROR("epoll_ctl: Unknown EventOperate: %d", handle_ptr->op);
            return -1;
        }
    }

    struct epoll_event events = event_type_convt(handle_ptr->type);
    events.data.fd = handle_ptr->tcp_conn->get_socket();

    int ret = epoll_ctl(epfd_, epoll_op, handle_ptr->tcp_conn->get_socket(), &events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    int fd = handle_ptr->tcp_conn->get_socket();
    if (handle_ptr->op == EventOperation_Del) {
        auto del_iter = client_.find(fd);
        if (del_iter != client_.end()) {
            LOG_INFO("Close client socket: %d", fd);
            client_.erase(del_iter);
            del_iter->second->tcp_conn->close();
        }
    } else if (handle_ptr->op == EventOperation_Add) {
        if (client_.find(fd) != client_.end()) {
            LOG_INFO("Client socket[%d] already exists", fd);
            return -1;
        }
        client_[fd] = handle_ptr;
        client_[fd]->is_send_ready = false; // 当前缓存置为false， 不发送
    }

    return ret;
}

void* 
SubReactor::event_wait(void *arg)
{
        if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    SubReactor *epoll_ptr = (SubReactor*)arg;
    while (epoll_ptr->exit_ == false) {
        int ret = ::epoll_wait(epoll_ptr->epfd_, epoll_ptr->events_, epoll_ptr->events_max_size_, epoll_ptr->timeout_);
        if (ret == -1) {
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            return nullptr;
        } else if (ret == 0) {
            continue;
        }

        epoll_ptr->recv_queue_mtx_->lock();
    
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
SubReactor::event_exit(void *arg)
{

}

void* 
SubReactor::event_send(void *arg)
{

}

}