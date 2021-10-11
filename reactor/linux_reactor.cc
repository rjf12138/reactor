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
MsgHandleCenter& 
MsgHandleCenter::instance(void)
{
    if (msg_handle_center_ == nullptr) {
        msg_handle_center_ = new MsgHandleCenter();
    }

    return *msg_handle_center_;
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

int 
MsgHandleCenter::add_send_task(ClientConn_t *client_ptr)
{
    os::Task task;
    task.work_func = send_client_data;
    task.thread_arg = client_ptr;

    return thread_pool_.add_priority_task(task);
}

void* MsgHandleCenter::send_client_data(void *arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    ClientConn_t *client_ptr = reinterpret_cast<ClientConn_t*>(arg);
    client_ptr->buff_mutex.lock();
    client_ptr->client_ptr->send(client_ptr->send_buffer);
    client_ptr->buff_mutex.unlock();

    return nullptr;
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

    MsgHandleCenter::instance().add_task(task);
}

SubReactor::~SubReactor(void)
{
    if (events_ != nullptr) {
        delete events_;
        events_ = nullptr;
    }
}

EventHandle_t*
SubReactor::get_event_handle(int client_sock)
{
    auto server_iter = client_conn_to_.find(client_sock);
    if (server_iter == client_conn_to_.end()) {
        return nullptr;
    }

    auto handle_iter = servers_.find(server_iter->second);
    if (handle_iter == servers_.end()) {
        return nullptr;
    }

    return handle_iter->second;
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

    servers_[handle_ptr->server_id] = handle_ptr;
    return 0;
}

int 
SubReactor::add_client_conn(server_id_t id, ClientConn_t *client_conn_ptr)
{
    if (client_conn_ptr == nullptr) {
        LOG_ERROR("client_ptr is nullptr");
        return -1;
    }

    auto find_iter = servers_.find(id);
    if (find_iter == servers_.end()) {
        LOG_ERROR("Can't find server id: 0x%x", id);
        return -1;
    }

    if (client_conn_ptr->client_ptr->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error client socket state: %d", client_conn_ptr->client_ptr->get_socket());
        return -1;
    }

    EventHandle_t *handle_ptr = find_iter->second;
    if (handle_ptr->acceptor->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error acceptor socket state: %d", handle_ptr->acceptor->get_socket());
        return -1;
    }

    // 添加客户端连接监听
    struct epoll_event ep_events = std_to_epoll_events(handle_ptr->events);
    ep_events.data.fd = client_conn_ptr->client_ptr->get_socket();

    int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, client_conn_ptr->client_ptr->get_socket(), &ep_events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    handle_ptr->client_conn_mutex.lock();
    handle_ptr->client_conn[client_conn_ptr->client_id] = client_conn_ptr;
    client_conn_to_[client_conn_ptr->client_ptr->get_socket()] = handle_ptr->server_id;
    handle_ptr->client_conn_mutex.unlock();

    return 0;
}

int 
SubReactor::remove_client_conn(server_id_t sid, client_id_t cid)
{
    auto find_iter = servers_.find(sid);
    if (find_iter == servers_.end()) {
        LOG_ERROR("Can't find server id: 0x%x", sid);
        return -1;
    }

    EventHandle_t *handle_ptr = find_iter->second;
    auto client_iter = handle_ptr->client_conn.find(cid);
    if (client_iter == handle_ptr->client_conn.end()) {
        return 0;
    }

    // 移除客户端连接监听
    struct epoll_event ep_events = std_to_epoll_events(handle_ptr->events);
    ep_events.data.fd = client_iter->second->client_ptr->get_socket();
    int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, client_iter->second->client_ptr->get_socket(), &ep_events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    ClientConn_t *del_conn_ptr = client_iter->second;
    auto client_conn_to_iter = client_conn_to_.find(client_iter->second->client_ptr->get_socket());
    handle_ptr->client_conn_mutex.lock();
    client_conn_to_.erase(client_conn_to_iter);
    handle_ptr->client_conn.erase(client_iter);
    handle_ptr->client_conn_mutex.unlock();

    delete del_conn_ptr; // 销毁ClientConn_t时会自动关闭连接
    
    return 0;
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
            int ready_socket_fd = epoll_ptr->events_[i].data.fd;
            EventHandle_t *handle_ptr = epoll_ptr->get_event_handle(ready_socket_fd);
            if (handle_ptr == nullptr) {
                LOG_GLOBAL_WARN("Cant find EventHandle of socket[%d]", ready_socket_fd);
                continue;
            }

            ClientConn_t *conn_ptr = handle_ptr->client_conn[ready_socket_fd];
            if (epoll_ptr->events_[i].events | EPOLLRDHUP) {
                LOG_GLOBAL_INFO("Client[%s] closed, remove client", conn_ptr->client_ptr->get_ip_info().c_str());
                epoll_ptr->remove_client_conn(handle_ptr->server_id, conn_ptr->client_id);
            } else if (epoll_ptr->events_[i].events | EPOLLERR) {
                LOG_GLOBAL_WARN("Client[%s] Error, remove client", sock_ptr->get_ip_info().c_str());
                epoll_ptr->remove_client_conn(handle_ptr->server_id, conn_ptr->client_id);
            } else if (epoll_ptr->events_[i].events | EPOLLHUP) {
                LOG_GLOBAL_INFO("Client[%s] closed read", conn_ptr->client_ptr->get_ip_info().c_str());
            } else {
                handle_ptr->ready_sock_mutex.lock();
                handle_ptr->ready_sock.push(ready_socket_fd);
                if (handle_ptr->state == EventHandleState_Idle) {
                    handle_ptr->state = EventHandleState_Ready;

                    os::Task task;
                    task.work_func = handle_ptr->client_func;
                    task.thread_arg = handle_ptr->client_arg;

                    MsgHandleCenter::instance().add_task(task);
                }
                handle_ptr->ready_sock_mutex.unlock();
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
MainReactor *MainReactor::s_main_reactor_ptr_ = nullptr;

MainReactor& 
MainReactor::instance(void)
{
    if (s_main_reactor_ptr_ == nullptr) {
        s_main_reactor_ptr_ = new MainReactor();
    }
    return *s_main_reactor_ptr_;
}

void
MainReactor::destory(void)
{
    if (s_main_reactor_ptr_ != nullptr) {
        delete s_main_reactor_ptr_;
        s_main_reactor_ptr_ = nullptr;
    }
}

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

    MsgHandleCenter::instance().add_task(task);
}

MainReactor::~MainReactor(void)
{
    if (events_ != nullptr) {
        delete events_;
        events_ = nullptr;
    }
}

int 
MainReactor::add_server_accept(EventHandle_t *handle_ptr)
{
    if (handle_ptr == nullptr) {
        LOG_ERROR("handle_ptr is nullptr");
        return -1;
    }

    if (handle_ptr->acceptor->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error socket state: %d", handle_ptr->acceptor->get_socket());
        return -1;
    }

    struct epoll_event ep_events;
    ep_events.events = EPOLLIN;
    ep_events.data.fd = handle_ptr->acceptor->get_socket();
    int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, handle_ptr->acceptor->get_socket(), &ep_events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    server_ctl_mutex_.lock();
    acceptor_[handle_ptr->server_id] = handle_ptr;
    sub_reactor_.server_register(handle_ptr);
    server_ctl_mutex_.unlock();

    return 0;
}

int 
MainReactor::remove_server_accept(server_id_t sid)
{
    auto accept_iter = acceptor_.find(sid);
    if (accept_iter == acceptor_.end()) {
        LOG_WARN("Can't find server id[%ld]", sid);
        return -1;
    }

    // 关闭服务端端口监听
    // 1. 关闭已经存在的所有客户端连接
    // 2. 关闭监听端口
    server_ctl_mutex_.lock();
    accept_iter->second->exit = true;
    auto iter = accept_iter->second->client_conn.begin();
    auto end_iter = accept_iter->second->client_conn.end();
    for (; iter != end_iter; ++iter) {
        sub_reactor_.remove_client_conn(sid, iter->first);
    }
    accept_iter->second->acceptor->close();
    acceptor_.erase(accept_iter);
    server_ctl_mutex_.unlock();

    return 0;
}

int 
MainReactor::remove_client_conn(server_id_t sid, client_id_t cid)
{
    return sub_reactor_.remove_client_conn(sid, cid);
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

            EventHandle_t *handle_ptr = find_iter->second;
            int client_sock_fd = 0;
            socklen_t addrlen = 0;
            struct sockaddr addr;
            if (handle_ptr->acceptor->accept(client_sock_fd, &addr, &addrlen) >= 0 && handle_ptr->exit == false) {
                ClientConn_t *client_conn_ptr = new ClientConn_t;
                client_conn_ptr->client_ptr->set_socket(client_sock_fd, (sockaddr_in*)&addr, &addrlen);
                epoll_ptr->sub_reactor_.add_client_conn(handle_ptr->acceptor->get_socket(), client_conn_ptr);

                if (handle_ptr->client_conn_func != nullptr) {
                    handle_ptr->client_conn_func(client_conn_ptr->client_id, handle_ptr->client_arg);
                }
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