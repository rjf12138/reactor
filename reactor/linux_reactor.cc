#include "linux_reactor.h"
#include "reactor.h"

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
ReactorManager& 
ReactorManager::instance(void)
{
    static ReactorManager s_msg_handle_center;
    return s_msg_handle_center;
}

ReactorManager::ReactorManager(void)
:
 main_reactor_ptr(nullptr)
{
    //thread_pool_.show_threadpool_info();
}

int 
ReactorManager::start(const ReactorConfig_t &config)
{
    this->set_config(config);

    if (main_reactor_ptr == nullptr && config.is_start_server == true) {
        main_reactor_ptr = new MainReactor();
        main_reactor_ptr->start();
    } else {
        LOG_GLOBAL_INFO("main reactor is already running or config start server is set false[%s]!", config.is_start_server ? "true":"false");
    }

    return 0;
}
int 
ReactorManager::stop(void)
{
    if (main_reactor_ptr != nullptr) {
        delete main_reactor_ptr;
        main_reactor_ptr = nullptr;
    }

    return 0;
}

ReactorManager::~ReactorManager(void)
{
    LOG_GLOBAL_INFO("~ReactorManager Stop.");
}

int 
ReactorManager::set_config(const ReactorConfig_t &config)
{
    if (config.sub_reactor_size_ < 5) {
        LOG_GLOBAL_WARN("The reactor requires at least 5 threads![Input: %d]", config.sub_reactor_size_);
        return -1;
    }
    os::ThreadPoolConfig threadpool_config = thread_pool_.get_threadpool_config();
    threadpool_config.threads_num = config.sub_reactor_size_ + 1;
    threadpool_config.max_waiting_task = 10000;
    thread_pool_.set_threadpool_config(threadpool_config);

    return 0;
}

int 
ReactorManager::add_task(os::Task &task)
{
    return thread_pool_.add_task(task);
}

int 
ReactorManager::add_timer(util::TimerEvent_t event)
{
    return timer_.add(event);
}

int 
ReactorManager::cancel_timer(int timer_id)
{
    return static_cast<int>(timer_.cancel(timer_id));
}

//////////////////////////////////////////////////////////////////////////////////////////
SubReactor::SubReactor(int events_max_size, int timeout)
:   timeout_(timeout),
    events_max_size_(events_max_size),
    reactor_state_(ReactorState_Exit)
{
    events_max_size_ = 32;
    events_ = new epoll_event[events_max_size_];

    epfd_ = epoll_create(5);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
    }
}

SubReactor::~SubReactor(void)
{
    this->stop();
}

int 
SubReactor::start(void)
{
    os::Task task;
    task.work_func = SubReactor::event_wait;
    task.thread_arg = this;
    task.exit_arg = this;

    reactor_state_ = ReactorState_Running;
    ReactorManager::instance().add_task(task);

    return 0;
}

int 
SubReactor::stop(void)
{
    reactor_state_ = ReactorState_WaitExit;
    while (reactor_state_ != ReactorState_Exit) {
        os::Time::sleep(100);
    }

    while (client_conn_.size() > 0) {
        this->remove_client_conn(client_conn_.begin()->first);
    }

    // 关闭epoll, 释放资源
    ::close(epfd_);
    if (events_ != nullptr) {
        delete []events_;
        events_ = nullptr;
    }

    return 0;
}

int 
SubReactor::add_client_conn(ClientConn_t *client_conn_ptr)
{
    if (client_conn_ptr == nullptr) {
        LOG_ERROR("client_ptr is nullptr");
        return -1;
    }

    if (client_conn_ptr->socket_ptr->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error client socket state: %d", client_conn_ptr->socket_ptr->get_socket());
        return -1;
    }

    // 添加客户端连接监听
    struct epoll_event event_;
    int sock_fd = client_conn_ptr->socket_ptr->get_socket();
    event_.data.fd = sock_fd;
    event_.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, client_conn_ptr->socket_ptr->get_socket(), &event_);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    mutex_.lock();
    client_conn_[sock_fd] = client_conn_ptr;
    mutex_.unlock();

    return 0;
}

int 
SubReactor::remove_client_conn(sock_id_t cid)
{
    auto  client_iter = client_conn_.find(cid);
    if (client_iter == client_conn_.end()) {
        LOG_ERROR("Can't find client id: 0x%x", cid);
        return -1;
    }

    ClientConn_t *del_conn_ptr = client_iter->second;
    mutex_.lock();
    client_conn_.erase(client_iter);
    mutex_.unlock();

    // 移除客户端连接监听
    struct epoll_event event_;
    event_.data.fd = del_conn_ptr->socket_ptr->get_socket();
    int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, del_conn_ptr->socket_ptr->get_socket(), &event_);
    if (ret == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    if (del_conn_ptr->is_client) {
        NetClient* connect_ptr = reinterpret_cast<NetClient*>(del_conn_ptr->client_arg);
        connect_ptr->set_state(NetConnectState_Disconnected);
        connect_ptr->notify_client_disconnected(cid);
    } else {
        NetServer* connect_ptr = reinterpret_cast<NetServer*>(del_conn_ptr->server_arg);
        connect_ptr->notify_client_disconnected(cid);
    }

    delete del_conn_ptr;                // 销毁ClientConn_t时会自动关闭连接

    return 0;
}

ssize_t 
SubReactor::send_data(sock_id_t cid, ByteBuffer &buffer)
{
    auto iter = client_conn_.find(cid);
    if (iter == client_conn_.end()) {
        LOG_GLOBAL_WARN("Send data failed[Can't find cid: %d]", cid);
        return -1;
    }

    return iter->second->socket_ptr->send(buffer);
}

void* 
SubReactor::event_wait(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    SubReactor *sub_reactor_ptr = reinterpret_cast<SubReactor*>(arg);
    while (sub_reactor_ptr->reactor_state_ == ReactorState_Running) {
        int ret = ::epoll_wait(sub_reactor_ptr->epfd_, sub_reactor_ptr->events_, sub_reactor_ptr->events_max_size_, 1000);
        if (ret < 0 && errno != EINTR) {
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            return nullptr;
        } else if (ret == 0) {
            continue;
        }

        for (int i = 0; i < ret; ++i) {
            int ready_socket_fd = sub_reactor_ptr->events_[i].data.fd;

            auto conn_iter = sub_reactor_ptr->client_conn_.find(ready_socket_fd);
            if (conn_iter == sub_reactor_ptr->client_conn_.end()) {
                LOG_GLOBAL_WARN("Client[%d] conn not found", ready_socket_fd);
                continue;
            }

            ClientConn_t *conn_ptr = conn_iter->second;
            if (sub_reactor_ptr->events_[i].events & EPOLLRDHUP) {
                LOG_GLOBAL_INFO("Client[%s] closed, remove client", conn_ptr->socket_ptr->get_ip_info().c_str());
                sub_reactor_ptr->remove_client_conn(conn_ptr->client_id);
            } else if (sub_reactor_ptr->events_[i].events & EPOLLERR) {
                LOG_GLOBAL_WARN("Client[%s] Error, remove client", conn_ptr->socket_ptr->get_ip_info().c_str());
                sub_reactor_ptr->remove_client_conn(conn_ptr->client_id);
            } else if (sub_reactor_ptr->events_[i].events & EPOLLHUP) {
                LOG_GLOBAL_INFO("Client[%s] closed read", conn_ptr->socket_ptr->get_ip_info().c_str());
            } else if (sub_reactor_ptr->events_[i].events & EPOLLIN) {
                conn_ptr->client_func(conn_ptr);
            }
        }
    }
    
    sub_reactor_ptr->reactor_state_ = ReactorState_Exit;
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
MainReactor::MainReactor(int sub_reactor_size)
:   sub_reactor_size_(sub_reactor_size),
    reactor_state_(ReactorState_Exit)
{
    events_max_size_ = 32;
    events_ = new epoll_event[events_max_size_];

    epfd_ = epoll_create(5);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
    }
}

MainReactor::~MainReactor(void)
{
    this->stop();
}

int 
MainReactor::start(void)
{
    for (int i = 0; i < sub_reactor_size_; ++i) {
        SubReactor *sub_reactor_ptr = new SubReactor();
        set_sub_reactors_.insert(sub_reactor_ptr);
        sub_reactor_ptr->start();
    }

    reactor_state_ = ReactorState_Running;

    os::Task task;
    task.work_func = MainReactor::event_wait;
    task.thread_arg = this;

    int ret = ReactorManager::instance().add_task(task);

    return ret;
}

int 
MainReactor::stop(void)
{
    reactor_state_ = ReactorState_WaitExit;
    // 等待监听线程退出
    while (reactor_state_ != ReactorState_Exit) {
        os::Time::sleep(50);
    }

    // 移除服务端
    while (acceptor_.size() > 0) {
        this->remove_server_accept(acceptor_.begin()->first);
    }

    // 关闭所有的sub reactor
    for (auto iter = set_sub_reactors_.begin(); iter != set_sub_reactors_.end(); ++iter) {
        delete *iter;
    }
    set_sub_reactors_.clear();

    // 关闭epoll, 释放资源
    ::close(epfd_);
    if (events_ != nullptr) {
        delete []events_;
        events_ = nullptr;
    }

    return 0;
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

    // 清空旧的数据
    server_ctl_mutex_.lock();
    mp_srv_clients_[handle_ptr->acceptor->get_socket()].clear();
    server_ctl_mutex_.unlock();

    struct epoll_event ep_events;
    memset(&ep_events, 0, sizeof(epoll_event));
    ep_events.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    ep_events.data.fd = handle_ptr->acceptor->get_socket();
    LOG_INFO("Reactor add server acceptor: [socket: %d]", ep_events.data.fd);
    int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, ep_events.data.fd, &ep_events);
    if (ret < 0) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    acceptor_[handle_ptr->acceptor->get_socket()] = handle_ptr;

    return 0;
}

int 
MainReactor::remove_server_accept(sock_id_t sid)
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
    for (auto client_iter = mp_srv_clients_[sid].begin(); client_iter != mp_srv_clients_[sid].end(); ++client_iter) {
        this->remove_client_conn(*client_iter);
    }
    accept_iter->second->acceptor->close();

    NetServer* connect_ptr = reinterpret_cast<NetServer*>(accept_iter->second->server_arg);
    connect_ptr->set_state(NetConnectState_Disconnected);
    connect_ptr->notify_server_stop_listen();

    acceptor_.erase(accept_iter);
    server_ctl_mutex_.unlock();

    return 0;
}

int 
MainReactor::remove_client_conn(sock_id_t cid)
{
    auto sub_reactor_iter = mp_cli_to_sub_reactor_.find(cid);
    if (sub_reactor_iter == mp_cli_to_sub_reactor_.end()) {
        return -1;
    }

    sub_reactor_iter->second->remove_client_conn(cid);
    return 0;
}

SubReactor * 
MainReactor::assign_conn_to_sub_reactor(ClientConn_t *conn)
{
    if (set_sub_reactors_.size() <= 0) {
        LOG_GLOBAL_WARN("SubReactor size is emptry!");
        return nullptr;
    }

    SubReactor *sub_ptr = *(set_sub_reactors_.begin());
    for (auto iter = set_sub_reactors_.begin(); iter != set_sub_reactors_.end(); ++iter) {
        if (sub_ptr->get_client_conn_size() > (*iter)->get_client_conn_size()) {
            sub_ptr = *iter;
        }
    }

    return sub_ptr;
}

SubReactor * 
MainReactor::get_client_sub_reactor(sock_id_t cid)
{
    auto iter = mp_cli_to_sub_reactor_.find(cid);
    if (iter == mp_cli_to_sub_reactor_.end()) {
        return nullptr;
    }

    return iter->second;
}

void* 
MainReactor::event_wait(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    MainReactor *main_reactor_ptr = reinterpret_cast<MainReactor*>(arg);
    while (main_reactor_ptr->reactor_state_ == ReactorState_Running) {
        int event_ret = ::epoll_wait(main_reactor_ptr->epfd_, main_reactor_ptr->events_, main_reactor_ptr->events_max_size_, 1000);
        if (event_ret < 0 && errno != EINTR) {
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            return nullptr;
        } else if (event_ret == 0) {
            continue;
        }

        for (int i = 0; i < event_ret; ++i) {
            int ready_socket_fd = main_reactor_ptr->events_[i].data.fd;
            EventHandle_t *handle_ptr = main_reactor_ptr->acceptor_[ready_socket_fd];
            if (handle_ptr == nullptr) {
                LOG_GLOBAL_WARN("Cant find EventHandle of socket[%d]", ready_socket_fd);
                continue;
            }

            int client_sock_fd = 0;
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            if (handle_ptr->acceptor->accept(client_sock_fd, reinterpret_cast<sockaddr*>(&addr), &addrlen) >= 0) {
                ClientConn_t *client_conn_ptr = new ClientConn_t;
                client_conn_ptr->client_id = client_sock_fd;
                client_conn_ptr->socket_ptr->set_socket(client_sock_fd, reinterpret_cast<sockaddr_in*>(&addr), &addrlen);
                client_conn_ptr->socket_ptr->setnonblocking();

                client_conn_ptr->server_arg = handle_ptr->server_arg;
                client_conn_ptr->client_func = handle_ptr->client_func;
                client_conn_ptr->connect_arg = handle_ptr->connect_arg;
                client_conn_ptr->client_conn_func = handle_ptr->client_conn_func;

                SubReactor* sub_reactor_ptr = main_reactor_ptr->assign_conn_to_sub_reactor(client_conn_ptr);
                if (sub_reactor_ptr == nullptr) {
                    client_conn_ptr->socket_ptr->close();
                    delete client_conn_ptr;
                    continue;
                }

                int reactor_ret = sub_reactor_ptr->add_client_conn(client_conn_ptr);
                if (reactor_ret < 0) {
                    client_conn_ptr->socket_ptr->close();
                    delete client_conn_ptr;
                    continue;
                }

                main_reactor_ptr->mp_cli_to_sub_reactor_[client_sock_fd] = sub_reactor_ptr;

                LOG_GLOBAL_INFO("Server add client[%s]", client_conn_ptr->socket_ptr->get_ip_info().c_str());
                if (handle_ptr->client_conn_func != nullptr) {
                    handle_ptr->client_conn_func(client_conn_ptr->client_id, handle_ptr->server_arg);
                }
            }
        }
    }

    main_reactor_ptr->reactor_state_ = ReactorState_Exit;
    
    return nullptr;
}

}