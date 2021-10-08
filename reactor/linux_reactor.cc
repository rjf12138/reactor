#include "linux_reactor.h"

namespace reactor {

#define CONVERT_TYPE(src, dst, x, y) \
{\
if (src & x) {\
    dst |= y;\
}\
}

struct epoll_event 
std_to_epoll_events(uint32_t type)
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

uint32_t
epoll_events_to_std(uint32_t events)
{
    uint32_t std_events = 0;
    CONVERT_TYPE(events, std_events, EPOLLIN, EventType_In);
    CONVERT_TYPE(events, std_events, EPOLLPRI, EventType_Pri);
    CONVERT_TYPE(events, std_events, EPOLLOUT, EventType_Out);
    CONVERT_TYPE(events, std_events, EPOLLRDHUP, EventType_RDHup);
    CONVERT_TYPE(events, std_events, EPOLLERR, EventType_Err);
    CONVERT_TYPE(events, std_events, EPOLLHUP, EventType_Hup);

    return std_events;
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
    threadpool_config.max_thread_num = config.max_thread_num;
    threadpool_config.min_thread_num = config.min_thread_num;
    thread_pool_.set_threadpool_config(threadpool_config);

    return 0;
}

int 
MsgHandleCenter::add_task(os::Task &task)
{
    return thread_pool_.add_task(task);
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

    os::Task task;
    task.work_func = SubReactor::event_wait;
    task.exit_task = SubReactor::event_exit;
    task.thread_arg = this;
    task.exit_arg = this;

    MsgHandleCenter::instance()->add_task(task);
}

SubReactor::~SubReactor(void)
{
    if (events_ != nullptr) {
        delete events_;
        events_ = nullptr;
    }
}

int 
SubReactor::server_register(EventHandle_t *handle_ptr)
{
    if (handle_ptr == nullptr) {
        LOG_ERROR("handle_ptr is nullptr");
        return -1;
    }

    if (handle_ptr->acceptor->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error acceptor socket state: %d", handle_ptr->acceptor->get_socket());
        return -1;
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

    uint32_t events = 0;
    if (handle_ptr->events.pop(events) <= 0) {
        return -1;
    }
    struct epoll_event ep_events = std_to_epoll_events(events);
    ep_events.data.fd = handle_ptr->tcp_conn->get_socket();

    int ret = epoll_ctl(epfd_, epoll_op, handle_ptr->tcp_conn->get_socket(), &ep_events);
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

        for (int i = 0; i < ret; ++i) {
            auto find_iter = epoll_ptr->client_.find(epoll_ptr->events_[i].data.fd);
            if (find_iter == epoll_ptr->client_.end()) {
                continue;
            }

            find_iter->second->event_mutex.lock();
            find_iter->second->events.push(epoll_events_to_std(epoll_ptr->events_[i].events));
            find_iter->second->event_mutex.unlock();

            if (find_iter->second->state == EventHandleState_Idle) {
                find_iter->second->is_send_ready = false; // 当前缓存置为false， 不发送消息
                find_iter->second->state = EventHandleState_Ready;

                os::Task task;
                task.work_func = find_iter->second->client_func;
                task.thread_arg = find_iter->second->client_arg;
                task.exit_task = nullptr;
                task.exit_arg = nullptr;

                MsgHandleCenter::instance()->add_task(task);
            }
        }
    }

    return nullptr;
}

void* 
SubReactor::event_exit(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    SubReactor *epoll_ptr = (SubReactor*)arg;
    epoll_ptr->exit_ = true;

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
MainReactor::MainReactor(int events_max_size, int timeout)
: events_max_size_(events_max_size),
  timeout_(timeout),
  exit_(false)
{
    if (events_max_size_ <= 0) {
        events_max_size_ = 32;
    }
    events_ = new epoll_event[events_max_size_];

    epfd_ = epoll_create(5);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
    }

    os::Task task;
    task.work_func = SubReactor::event_wait;
    task.exit_task = SubReactor::event_exit;
    task.thread_arg = this;
    task.exit_arg = this;

    MsgHandleCenter::instance()->add_task(task);
}

MainReactor::~MainReactor(void)
{
    if (events_ != nullptr) {
        delete events_;
        events_ = nullptr;
    }
}

int 
MainReactor::client_ctl(EventHandle_t *handle_ptr)
{
    if (handle_ptr == nullptr) {
        LOG_ERROR("handle_ptr is nullptr");
        return -1;
    }

    if (handle_ptr->acceptor->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error socket state: %d", handle_ptr->acceptor->get_socket());
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

    uint32_t events = 0;
    if (handle_ptr->events.pop(events) <= 0) {
        return -1;
    }
    struct epoll_event ep_events = std_to_epoll_events(events);
    ep_events.data.fd = handle_ptr->acceptor->get_socket();

    int ret = epoll_ctl(epfd_, epoll_op, handle_ptr->acceptor->get_socket(), &ep_events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    int fd = handle_ptr->acceptor->get_socket();
    if (handle_ptr->op == EventOperation_Del) {
        auto del_iter = acceptor_.find(fd);
        if (del_iter != acceptor_.end()) {   // TODO: 关闭服务器前关闭所有客户端连接
            LOG_INFO("Close client socket: %d", fd);
            acceptor_.erase(del_iter);
            del_iter->second->acceptor->close();
        }
    } else if (handle_ptr->op == EventOperation_Add) {
        if (acceptor_.find(fd) != acceptor_.end()) {
            LOG_INFO("Client socket[%d] already exists", fd);
            return -1;
        }
        acceptor_[fd] = handle_ptr;
        acceptor_[fd]->is_send_ready = false; // 当前缓存置为false， 不发送
    }

    return ret;
}

void* 
MainReactor::event_wait(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    MainReactor *epoll_ptr = (MainReactor*)arg;
    while (epoll_ptr->exit_ == false) {
        int ret = ::epoll_wait(epoll_ptr->epfd_, epoll_ptr->events_, epoll_ptr->events_max_size_, epoll_ptr->timeout_);
        if (ret == -1) {
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            return nullptr;
        } else if (ret == 0) {
            continue;
        }

        for (int i = 0; i < ret; ++i) {
            auto find_iter = epoll_ptr->acceptor_.find(epoll_ptr->events_[i].data.fd);
            if (find_iter == epoll_ptr->acceptor_.end()) {
                continue;
            }

            int client_sock_fd = 0;
            socklen_t addrlen = 0;
            struct sockaddr addr;
            if (find_iter->second->acceptor->accept(client_sock_fd, &addr, &addrlen) >= 0) {
                os::SocketTCP *client_ptr = new os::SocketTCP();
                client_ptr->set_socket(client_sock_fd, (sockaddr_in*)&addr, &addrlen);
                epoll_ptr->sub_reactor_.add_client_conn(client_ptr);
            }
        }
    }

    return nullptr;
}

void* 
MainReactor::event_exit(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    MainReactor *epoll_ptr = (MainReactor*)arg;
    epoll_ptr->exit_ = true;

    return nullptr;
}

}