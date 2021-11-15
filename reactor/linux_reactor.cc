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
    CONVERT_TYPE(type, event.events, EventType_ET, EPOLLET);

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
MsgHandleCenter& 
MsgHandleCenter::instance(void)
{
    static MsgHandleCenter s_msg_handle_center;
    return s_msg_handle_center;
}

MsgHandleCenter::MsgHandleCenter(void)
{
    //thread_pool_.show_threadpool_info();
    ReactorConfig rconfig;
    rconfig.threads_num = 5;
    rconfig.max_wait_task = 1000;
    this->set_config(rconfig);
}

MsgHandleCenter::~MsgHandleCenter(void)
{
}

int 
MsgHandleCenter::set_config(const ReactorConfig_t &config)
{
    if (config.threads_num < 5) {
        LOG_GLOBAL_WARN("The reactor requires at least 5 threads![Input: %d]", config.threads_num);
        return -1;
    }
    os::ThreadPoolConfig threadpool_config = thread_pool_.get_threadpool_config();
    threadpool_config.threads_num = config.threads_num;
    threadpool_config.max_waiting_task = config.max_wait_task;
    thread_pool_.set_threadpool_config(threadpool_config);

    return 0;
}

int 
MsgHandleCenter::add_task(os::Task &task)
{
    return thread_pool_.add_task(task);
}

int 
MsgHandleCenter::add_timer(util::TimerEvent_t event)
{
    return timer_.add(event);
}

int 
MsgHandleCenter::cancel_timer(int timer_id)
{
    return timer_.cancel(timer_id);
}

//////////////////////////////////////////////////////////////////////////////////////////
SendDataCenter& 
SendDataCenter::instance(void)
{
    static SendDataCenter send_data_center_;
    return send_data_center_;
}

SendDataCenter::SendDataCenter(void)
:send_exit_(false)
{
    os::Task task;
    task.work_func = send_loop;
    task.thread_arg = this;
    task.exit_task = exit_loop;
    task.exit_arg = this;

    MsgHandleCenter::instance().add_task(task);
}

SendDataCenter::~SendDataCenter(void)
{
}

int 
SendDataCenter::send_data(client_id_t cid)
{
    send_mtx_.lock();
    send_queue_.push(cid);
    send_mtx_.unlock();

    return 0;
}

int 
SendDataCenter::register_connection(ClientConn_t *client_ptr)
{
    conn_mtx_.lock();
    sender_conns_[client_ptr->client_id] = client_ptr;
    conn_mtx_.unlock();
    return 1;
}

int 
SendDataCenter::remove_connection(client_id_t cid)
{
    auto iter = sender_conns_.find(cid);
    if (iter == sender_conns_.end()) {
        return 0;
    }

    conn_mtx_.lock();
    sender_conns_.erase(iter);
    conn_mtx_.unlock();

    return 1;
}

void* 
SendDataCenter::send_loop(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    SendDataCenter *sender_ptr = reinterpret_cast<SendDataCenter*>(arg);
    while (sender_ptr->send_exit_ == false) {
        if (sender_ptr->send_queue_.size() <= 0) {
            os::Time::sleep(5);
        }

        client_id_t cid;
        while (sender_ptr->send_queue_.size() > 0) {
            sender_ptr->send_mtx_.lock();
            sender_ptr->send_queue_.pop(cid);
            sender_ptr->send_mtx_.unlock();

            auto iter = sender_ptr->sender_conns_.find(cid);
            if (iter == sender_ptr->sender_conns_.end()) {
                LOG_GLOBAL_WARN("Can't find sender object[%d]", cid);
                continue;
            }

            if (iter->second->send_buffer.data_size() == 0) {
                continue;
            }
            iter->second->socket_ptr->send(iter->second->send_buffer);
        }
    }
    return nullptr;
}

void* 
SendDataCenter::exit_loop(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    SendDataCenter *sender_ptr = reinterpret_cast<SendDataCenter*>(arg);
    sender_ptr->send_exit_ = true;

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
SubReactor& 
SubReactor::instance(void)
{
    static SubReactor s_sub_reactor_;
    return s_sub_reactor_;
}

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
    } else {
        os::Task task;
        task.work_func = SubReactor::event_wait;
        task.exit_task = SubReactor::event_exit;
        task.thread_arg = this;
        task.exit_arg = this;

        MsgHandleCenter::instance().add_task(task);
    }
}

SubReactor::~SubReactor(void)
{
    if (events_ != nullptr) {
        delete []events_;
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

    // 如果设置了 acceptor 需要检查它的状态是不是连接着的
    if (handle_ptr->acceptor != nullptr && handle_ptr->acceptor->get_socket_state() == false) {
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

    if (client_conn_ptr->socket_ptr->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error client socket state: %d", client_conn_ptr->socket_ptr->get_socket());
        return -1;
    }

    EventHandle_t *handle_ptr = find_iter->second;
    // 如果设置了 acceptor 需要检查它的状态是不是连接着的
    if (handle_ptr->acceptor != nullptr && handle_ptr->acceptor->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error acceptor socket state: %d", handle_ptr->acceptor->get_socket());
        return -1;
    }

    // 添加客户端连接监听
    struct epoll_event ep_events = std_to_epoll_events(handle_ptr->events);
    ep_events.data.fd = client_conn_ptr->socket_ptr->get_socket();
    int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, client_conn_ptr->socket_ptr->get_socket(), &ep_events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    handle_ptr->client_conn_mutex.lock();
    handle_ptr->client_conn[client_conn_ptr->client_id] = client_conn_ptr;
    client_conn_to_[client_conn_ptr->socket_ptr->get_socket()] = handle_ptr->server_id;
    handle_ptr->client_conn_mutex.unlock();

    SendDataCenter::instance().register_connection(client_conn_ptr);

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
        LOG_ERROR("Can't find client id: 0x%x", cid);
        return 0;
    }

    // 移除客户端连接监听
    struct epoll_event ep_events = std_to_epoll_events(handle_ptr->events);
    ep_events.data.fd = client_iter->second->socket_ptr->get_socket();
    int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, client_iter->second->socket_ptr->get_socket(), &ep_events);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    ClientConn_t *del_conn_ptr = client_iter->second;
    auto client_conn_to_iter = client_conn_to_.find(client_iter->second->socket_ptr->get_socket());
    handle_ptr->client_conn_mutex.lock();
    client_conn_to_.erase(client_conn_to_iter);
    handle_ptr->client_conn.erase(client_iter);
    handle_ptr->client_conn_mutex.unlock();

    delete del_conn_ptr; // 销毁ClientConn_t时会自动关闭连接
    
    if (handle_ptr->acceptor == nullptr) { // 客户端acceptor 是nullptr
        NetClient* connect_ptr = reinterpret_cast<NetClient*>(handle_ptr->server_id);
        connect_ptr->set_state(NetConnectState_Dissconnected);
        connect_ptr->notify_client_disconnected(cid);
    } else {
        NetServer* connect_ptr = reinterpret_cast<NetServer*>(handle_ptr->server_id);
        connect_ptr->notify_client_disconnected(cid);
    }

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
        if (ret == -1 && errno != EINTR) {
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
            if (epoll_ptr->events_[i].events & EPOLLRDHUP) {
                LOG_GLOBAL_INFO("Client[%s] closed, remove client", conn_ptr->socket_ptr->get_ip_info().c_str());
                epoll_ptr->remove_client_conn(handle_ptr->server_id, conn_ptr->client_id);
            } else if (epoll_ptr->events_[i].events & EPOLLERR) {
                LOG_GLOBAL_WARN("Client[%s] Error, remove client", conn_ptr->socket_ptr->get_ip_info().c_str());
                epoll_ptr->remove_client_conn(handle_ptr->server_id, conn_ptr->client_id);
            } else if (epoll_ptr->events_[i].events & EPOLLHUP) {
                LOG_GLOBAL_INFO("Client[%s] closed read", conn_ptr->socket_ptr->get_ip_info().c_str());
            } else if (epoll_ptr->events_[i].events & EPOLLIN){
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
MainReactor& 
MainReactor::instance(void)
{
    static MainReactor s_main_reactor_;
    return s_main_reactor_;
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
    task.work_func = MainReactor::event_wait;
    task.exit_task = MainReactor::event_exit;
    task.thread_arg = this;
    task.exit_arg = this;

    MsgHandleCenter::instance().add_task(task);
}

MainReactor::~MainReactor(void)
{
    if (events_ != nullptr) {
        delete []events_;
        events_ = nullptr;
    }
}

EventHandle_t* 
MainReactor::get_event_handle(int listen_socket_fd)
{
    auto server_iter = server_listen_to_.find(listen_socket_fd);
    if (server_iter == server_listen_to_.end()) {
        return nullptr;
    }

    auto handle_iter = acceptor_.find(server_iter->second);
    if (handle_iter == acceptor_.end()) {
        return nullptr;
    }

    return handle_iter->second;
}

int 
MainReactor::add_server_accept(EventHandle_t *handle_ptr)
{
    if (handle_ptr == nullptr || handle_ptr->acceptor == nullptr) {
        LOG_ERROR("handle_ptr is nullptr");
        return -1;
    }

    if (handle_ptr->acceptor->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error socket state: %d", handle_ptr->acceptor->get_socket());
        return -1;
    }

    struct epoll_event ep_events;
    memset(&ep_events, 0, sizeof(epoll_event));
    ep_events.events = EPOLLIN | EPOLLERR;
    ep_events.data.fd = handle_ptr->acceptor->get_socket();
    LOG_INFO("Reactor add server acceptor: [socket: %d]", ep_events.data.fd);
    int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, ep_events.data.fd, &ep_events);
    if (ret < 0) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    server_ctl_mutex_.lock();
    server_listen_to_[handle_ptr->acceptor->get_socket()] = handle_ptr->server_id;
    acceptor_[handle_ptr->server_id] = handle_ptr;
    SubReactor::instance().server_register(handle_ptr);
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
    for (; iter != end_iter; ) {
        auto stop_iter = iter++;
        SubReactor::instance().remove_client_conn(sid, stop_iter->first);
    }
    accept_iter->second->acceptor->close();
    acceptor_.erase(accept_iter);
    server_ctl_mutex_.unlock();

    return 0;
}

int 
MainReactor::remove_client_conn(server_id_t sid, client_id_t cid)
{
    return SubReactor::instance().remove_client_conn(sid, cid);
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
        if (ret < 0 && errno != EINTR) {
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

            int client_sock_fd = 0;
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            if (handle_ptr->acceptor->accept(client_sock_fd, (sockaddr*)&addr, &addrlen) >= 0 && handle_ptr->exit == false) {
                ClientConn_t *client_conn_ptr = new ClientConn_t;
                client_conn_ptr->client_id = client_sock_fd;
                client_conn_ptr->socket_ptr->set_socket(client_sock_fd, (sockaddr_in*)&addr, &addrlen);
                client_conn_ptr->socket_ptr->setnonblocking();
                int ret = SubReactor::instance().add_client_conn(handle_ptr->server_id, client_conn_ptr);
                if (ret < 0) {
                    client_conn_ptr->socket_ptr->close();
                    delete client_conn_ptr;
                    continue;
                }
                LOG_GLOBAL_INFO("Server add client[%s]", client_conn_ptr->socket_ptr->get_ip_info().c_str());
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