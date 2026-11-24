#include "linux_reactor.h"
#include "reactor.h"

#include <cstring>
#include <new>

namespace reactor {

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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (main_reactor_ptr != nullptr) {
            LOG_GLOBAL_INFO("main reactor is already running!");
            return 0;
        }
    }

    if (this->set_config(config) != 0) {
        LOG_GLOBAL_ERROR("Reactor config is invalid, start reactor failed.");
        return -1;
    }

    if (config.is_start_server == false) {
        LOG_GLOBAL_INFO("config start server is set false, reactor not started.");
        return 0;
    }

    MainReactor *reactor_ptr = new (std::nothrow) MainReactor(config.sub_reactor_size_,
                                                              config.main_reactor_max_events_size_,
                                                              config.sub_reactor_max_events_size_);
    if (reactor_ptr == nullptr) {
        LOG_GLOBAL_ERROR("Alloc MainReactor failed.");
        return -1;
    }

    if (reactor_ptr->start() != 0) {
        LOG_GLOBAL_ERROR("Start main reactor failed.");
        delete reactor_ptr;
        return -1;
    }

    bool keep = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (main_reactor_ptr == nullptr) {
            main_reactor_ptr = reactor_ptr;
            keep = true;
        }
    }

    if (keep == false) {
        // 并发调用 start，保留已经运行的那个
        LOG_GLOBAL_WARN("main reactor is already running, ignore duplicate start.");
        delete reactor_ptr;
    }

    return 0;
}

int 
ReactorManager::stop(void)
{
    MainReactor *reactor_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        reactor_ptr = main_reactor_ptr;
        main_reactor_ptr = nullptr;
    }

    if (reactor_ptr != nullptr) {
        delete reactor_ptr;
    }

    return 0;
}

ReactorManager::~ReactorManager(void)
{
    LOG_GLOBAL_INFO("~ReactorManager Stop.");
    this->stop();
}

int 
ReactorManager::set_config(const ReactorConfig_t &config)
{
    if (config.sub_reactor_size_ < 5 ||
        config.sub_reactor_size_ > static_cast<uint32_t>(MAX_THREADS_NUM - 4)) {
        LOG_GLOBAL_WARN("The reactor requires at least 5 threads and at most %d![Input: %u]",
                        MAX_THREADS_NUM - 4, config.sub_reactor_size_);
        return -1;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    os::ThreadPoolConfig threadpool_config = thread_pool_.get_threadpool_config();
    threadpool_config.threads_num = config.sub_reactor_size_ + 4;
    threadpool_config.max_waiting_task = 10000;
    threadpool_config.threadpool_exit_action = os::SHUTDOWN_ALL_THREAD_IMMEDIATELY;
    return thread_pool_.set_threadpool_config(threadpool_config);
}

int 
ReactorManager::add_task(os::Task &task)
{
    return thread_pool_.add_task(task);
}

int 
ReactorManager::add_timer(util::TimerEvent_t event)
{
    return static_cast<int>(timer_.add(event));
}

int 
ReactorManager::cancel_timer(int timer_id)
{
    return static_cast<int>(timer_.cancel(static_cast<util::timer_id_t>(timer_id)));
}

//////////////////////////////////////////////////////////////////////////////////////////
SubReactor::SubReactor(int events_max_size)
:  epfd_(-1),
   events_max_size_(events_max_size),
   events_(nullptr),
   reactor_state_(ReactorState_Exit)
{
    events_max_size_ = events_max_size_ < 32 ? 32 : events_max_size_;
    events_ = new (std::nothrow) epoll_event[events_max_size_];
    if (events_ == nullptr) {
        LOG_ERROR("Alloc epoll_event failed.");
        events_max_size_ = 0;
        reactor_state_ = ReactorState_Error;
        return;
    }

    epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
        delete []events_;
        events_ = nullptr;
        events_max_size_ = 0;
        reactor_state_ = ReactorState_Error;
    }
}

SubReactor::~SubReactor(void)
{
    this->stop();
}

int 
SubReactor::start(void)
{
    if (reactor_state_.load() == ReactorState_Running) {
        return 0;
    }
    if (epfd_ < 0 || events_ == nullptr) {
        LOG_ERROR("SubReactor not initialized.");
        return -1;
    }

    os::Task task;
    task.work_func = SubReactor::event_wait;
    task.thread_arg = this;
    task.exit_arg = this;

    reactor_state_ = ReactorState_Running;
    if (ReactorManager::instance().add_task(task) <= 0) {
        // 任务添加失败（线程池队列满或已停止），事件线程不会运行，
        // 必须回滚状态，否则 stop() 会一直等待。
        reactor_state_ = ReactorState_Exit;
        LOG_ERROR("Add sub reactor task failed.");
        return -1;
    }

    return 0;
}

int 
SubReactor::stop(void)
{
    ReactorState old_state = reactor_state_.load();
    if (old_state == ReactorState_Exit) {
        // 事件线程没有运行（未启动或已经退出），直接清理资源
        std::unique_lock<std::mutex> lock(mutex_);
        std::map<sock_id_t, ClientConn_t*> old_conns;
        old_conns.swap(client_conn_);
        lock.unlock();

        for (auto iter = old_conns.begin(); iter != old_conns.end(); ++iter) {
            this->notify_disconnected(iter->second);
            delete iter->second;
        }

        if (epfd_ >= 0) {
            ::close(epfd_);
            epfd_ = -1;
        }
        if (events_ != nullptr) {
            delete []events_;
            events_ = nullptr;
        }
        reactor_state_ = ReactorState_Exit;
        return 0;
    }

    if (old_state == ReactorState_Error) {
        reactor_state_ = ReactorState_Exit;
        return -1;
    }

    reactor_state_ = ReactorState_WaitExit;

    // 等待事件线程退出，最多等待 5 秒，避免任务没有执行时永久阻塞
    bool exited = false;
    for (int i = 0; i < 50; ++i) {
        if (reactor_state_.load() == ReactorState_Exit) {
            exited = true;
            break;
        }
        os::Time::sleep(100);
    }

    if (exited == false) {
        // 事件线程仍在使用 epoll 资源和连接对象，此时不能释放资源，
        // 返回失败由上层决定（宁可泄漏也不能释放后继续被访问）。
        LOG_GLOBAL_ERROR("SubReactor event thread did not exit in time.");
        return -1;
    }

    while (true) {
        sock_id_t cid = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (client_conn_.empty()) {
                break;
            }
            cid = client_conn_.begin()->first;
        }
        this->remove_client_conn(cid);
    }

    // 关闭epoll, 释放资源
    if (epfd_ >= 0) {
        ::close(epfd_);
        epfd_ = -1;
    }
    if (events_ != nullptr) {
        delete []events_;
        events_ = nullptr;
    }

    return 0;
}
int 
SubReactor::get_client_conn_size(void)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(client_conn_.size());
}

ClientConn_t*
SubReactor::acquire_client_conn(sock_id_t cid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = client_conn_.find(cid);
    if (iter == client_conn_.end()) {
        return nullptr;
    }

    ClientConn_t *conn_ptr = iter->second;
    if (conn_ptr->closed.load() == true) {
        return nullptr;
    }

    // 标记正在处理，remove_client_conn 会延迟销毁，避免回调中释放连接
    conn_ptr->in_callback = true;
    return conn_ptr;
}

void 
SubReactor::release_client_conn(ClientConn_t *conn_ptr)
{
    if (conn_ptr == nullptr) {
        return;
    }

    bool need_delete = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn_ptr->in_callback = false;
        if (conn_ptr->pending_delete) {
            conn_ptr->pending_delete = false;
            need_delete = true;
        }
    }

    if (need_delete) {
        delete conn_ptr;
    }
}

ssize_t 
SubReactor::flush_send_buffer(ClientConn_t *conn_ptr)
{
    if (conn_ptr == nullptr || conn_ptr->socket_ptr == nullptr) {
        return -1;
    }
    if (conn_ptr->send_buffer.data_size() <= 0) {
        return 0;
    }

    ssize_t ret = conn_ptr->socket_ptr->send(conn_ptr->send_buffer);
    if (ret < 0) {
        LOG_GLOBAL_WARN("Send data failed, client: %s", conn_ptr->socket_ptr->get_ip_info().c_str());
        conn_ptr->send_buffer.clear();
    }

    return ret;
}

int 
SubReactor::update_epoll_out(ClientConn_t *conn_ptr, bool enable)
{
    if (conn_ptr == nullptr || conn_ptr->socket_ptr == nullptr || epfd_ < 0) {
        return -1;
    }
    if (conn_ptr->epoll_out_enabled == enable) {
        return 0;
    }

    struct epoll_event event_;
    memset(&event_, 0, sizeof(event_));
    event_.data.fd = conn_ptr->socket_ptr->get_socket();
    event_.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    if (enable) {
        event_.events |= EPOLLOUT;
    }

    if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, event_.data.fd, &event_) == -1) {
        LOG_ERROR("epoll_ctl MOD: %s", strerror(errno));
        return -1;
    }

    conn_ptr->epoll_out_enabled = enable;
    return 0;
}

void 
SubReactor::notify_disconnected(ClientConn_t *conn_ptr)
{
    if (conn_ptr == nullptr) {
        return;
    }

    try {
        if (conn_ptr->is_client) {
            NetClient *client_ptr = reinterpret_cast<NetClient*>(conn_ptr->client_arg);
            if (client_ptr != nullptr) {
                client_ptr->set_state(NetConnectState_Disconnected);
                client_ptr->notify_client_disconnected(conn_ptr->client_id);
            }
        } else {
            NetServer *server_ptr = reinterpret_cast<NetServer*>(conn_ptr->server_arg);
            if (server_ptr != nullptr) {
                server_ptr->notify_client_disconnected(conn_ptr->client_id);
            }
        }
    } catch (...) {
        LOG_GLOBAL_ERROR("notify client disconnected callback threw exception.");
    }
}



int 
SubReactor::add_client_conn(ClientConn_t *client_conn_ptr)
{
    if (client_conn_ptr == nullptr || client_conn_ptr->socket_ptr == nullptr) {
        LOG_ERROR("client_ptr is nullptr");
        return -1;
    }

    if (client_conn_ptr->socket_ptr->get_socket_state() == false) {
        LOG_ERROR("get_socket_state: Error client socket state: %d", client_conn_ptr->socket_ptr->get_socket());
        return -1;
    }

    if (reactor_state_.load() != ReactorState_Running) {
        LOG_WARN("SubReactor is not running, can not add client.");
        return -1;
    }

    int sock_fd = client_conn_ptr->socket_ptr->get_socket();
    if (sock_fd < 0) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (client_conn_.find(sock_fd) != client_conn_.end()) {
        LOG_ERROR("Client[%d] already exists.", sock_fd);
        return -1;
    }

    // 添加客户端连接监听
    struct epoll_event event_;
    memset(&event_, 0, sizeof(event_));
    event_.data.fd = sock_fd;
    event_.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, sock_fd, &event_) == -1) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    client_conn_ptr->closed.store(false);
    client_conn_ptr->epoll_out_enabled = false;
    client_conn_[sock_fd] = client_conn_ptr;

    return 0;
}

int 
SubReactor::remove_client_conn(sock_id_t cid)
{
    ClientConn_t *del_conn_ptr = nullptr;
    bool delete_now = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto client_iter = client_conn_.find(cid);
        if (client_iter == client_conn_.end()) {
            LOG_GLOBAL_WARN("Can't find client id: 0x%llx", static_cast<unsigned long long>(cid));
            return -1;
        }

        del_conn_ptr = client_iter->second;
        client_conn_.erase(client_iter);
        del_conn_ptr->closed.store(true);

        if (del_conn_ptr->in_callback) {
            // 事件线程正在使用该连接，延迟到回调返回后再销毁
            del_conn_ptr->pending_delete = true;
        } else {
            delete_now = true;
        }
    }

    // 移除客户端连接监听（即使失败也继续清理，fd 可能已经被关闭）
    struct epoll_event event_;
    memset(&event_, 0, sizeof(event_));
    event_.data.fd = static_cast<int>(cid);
    if (epfd_ >= 0) {
        if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, static_cast<int>(cid), &event_) == -1 &&
            errno != EBADF && errno != ENOENT) {
            LOG_ERROR("epoll_ctl: %s", strerror(errno));
        }
    }

    this->notify_disconnected(del_conn_ptr);

    // 通知 MainReactor 清理连接关系表
    if (del_conn_ptr->close_func != nullptr) {
        try {
            del_conn_ptr->close_func(del_conn_ptr->client_id, del_conn_ptr->close_arg);
        } catch (...) {
            LOG_GLOBAL_ERROR("client close callback threw exception.");
        }
    }

    LOG_GLOBAL_INFO("Client[%s] closed, remove client", del_conn_ptr->socket_ptr->get_ip_info().c_str());
    if (delete_now) {
        delete del_conn_ptr;            // 销毁ClientConn_t时会自动关闭连接
    }

    return 0;
}

int 
SubReactor::close_client_gracefully(sock_id_t cid)
{
    bool need_close = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = client_conn_.find(cid);
        if (iter == client_conn_.end()) {
            LOG_GLOBAL_WARN("Can't find client id: 0x%llx", static_cast<unsigned long long>(cid));
            return -1;
        }

        ClientConn_t *conn_ptr = iter->second;
        if (conn_ptr->closed.load() == true) {
            return 0;
        }

        conn_ptr->close_after_send = true;
        if (conn_ptr->send_buffer.data_size() <= 0) {
            // 数据已全部发送，直接关闭（不能持锁调用 remove_client_conn）
            need_close = true;
        } else {
            // 停止接收新数据，只等待 EPOLLOUT 把剩余数据发完。
            // 不注册 EPOLLRDHUP，避免半关闭连接在数据发完前反复触发关闭事件。
            struct epoll_event event_;
            memset(&event_, 0, sizeof(event_));
            event_.data.fd = conn_ptr->socket_ptr->get_socket();
            event_.events = EPOLLOUT | EPOLLERR;
            if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, event_.data.fd, &event_) == 0) {
                conn_ptr->epoll_out_enabled = true;
            } else {
                LOG_ERROR("epoll_ctl MOD: %s", strerror(errno));
            }
        }
    }

    if (need_close) {
        return this->remove_client_conn(cid);
    }

    return 0;
}

ssize_t 
SubReactor::send_data(sock_id_t cid, ByteBuffer &buffer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = client_conn_.find(cid);
    if (iter == client_conn_.end()) {
        LOG_GLOBAL_WARN("Send data failed[Can't find cid: %llx]", static_cast<unsigned long long>(cid));
        return -1;
    }

    ClientConn_t *conn_ptr = iter->second;
    if (conn_ptr->closed.load() == true || conn_ptr->socket_ptr == nullptr ||
        conn_ptr->close_after_send == true) {
        return -1;
    }

    if (buffer.data_size() > 0) {
        if (conn_ptr->send_buffer.data_size() + buffer.data_size() > MAX_SEND_BUFFER_SIZE) {
            LOG_GLOBAL_WARN("Send buffer is full[cid: %llx, cached: %ld], drop data.",
                            static_cast<unsigned long long>(cid),
                            conn_ptr->send_buffer.data_size());
            return -1;
        }
        conn_ptr->send_buffer += buffer;
    }

    ssize_t sent = this->flush_send_buffer(conn_ptr);
    if (sent < 0) {
        return -1;
    }

    if (conn_ptr->send_buffer.data_size() > 0) {
        // 还有剩余数据，等 EPOLLOUT 继续发送
        if (this->update_epoll_out(conn_ptr, true) != 0) {
            return -1;
        }
    } else {
        this->update_epoll_out(conn_ptr, false);
    }

    // 返回已被接受的字节数（数据已放入发送队列）
    return buffer.data_size();
}

void* 
SubReactor::event_wait(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    SubReactor *sub_reactor_ptr = reinterpret_cast<SubReactor*>(arg);
    while (sub_reactor_ptr->reactor_state_.load() == ReactorState_Running) {
        int ret = ::epoll_wait(sub_reactor_ptr->epfd_, sub_reactor_ptr->events_,
                               sub_reactor_ptr->events_max_size_, 300);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            break;
        } else if (ret == 0) {
            continue;
        }

        for (int i = 0; i < ret; ++i) {
            int ready_socket_fd = sub_reactor_ptr->events_[i].data.fd;
            uint32_t ready_events = sub_reactor_ptr->events_[i].events;
            bool fatal_event = ((ready_events & (EPOLLERR | EPOLLHUP)) != 0);
            bool read_half_closed = ((ready_events & EPOLLRDHUP) != 0);

            // 先处理可读数据，再处理可写（部分发送的剩余数据），最后处理关闭事件，
            // 避免连接关闭时丢掉内核中已经到达的数据。
            if ((ready_events & EPOLLIN) != 0) {
                ClientConn_t *conn_ptr = sub_reactor_ptr->acquire_client_conn(ready_socket_fd);
                if (conn_ptr != nullptr) {
                    if (conn_ptr->closed.load() == false && conn_ptr->client_func != nullptr) {
                        try {
                            conn_ptr->client_func(conn_ptr);
                        } catch (const std::exception &e) {
                            LOG_GLOBAL_ERROR("client handler threw exception: %s", e.what());
                        } catch (...) {
                            LOG_GLOBAL_ERROR("client handler threw unknown exception.");
                        }
                    }
                    sub_reactor_ptr->release_client_conn(conn_ptr);
                }
            }

            if ((ready_events & EPOLLOUT) != 0) {
                ClientConn_t *conn_ptr = sub_reactor_ptr->acquire_client_conn(ready_socket_fd);
                if (conn_ptr != nullptr) {
                    bool need_close = false;
                    {
                        std::lock_guard<std::mutex> lock(sub_reactor_ptr->mutex_);
                        if (conn_ptr->closed.load() == false) {
                            sub_reactor_ptr->flush_send_buffer(conn_ptr);
                            if (conn_ptr->send_buffer.data_size() <= 0) {
                                if (conn_ptr->close_after_send) {
                                    need_close = true;
                                } else {
                                    sub_reactor_ptr->update_epoll_out(conn_ptr, false);
                                }
                            }
                        }
                    }
                    if (need_close) {
                        sub_reactor_ptr->remove_client_conn(ready_socket_fd);
                    }
                    sub_reactor_ptr->release_client_conn(conn_ptr);
                }
            }

            if (fatal_event == true) {
                sub_reactor_ptr->remove_client_conn(ready_socket_fd);
            } else if (read_half_closed == true) {
                // 对端关闭了写端（例如 HTTP 客户端 shutdown(SHUT_WR) 后等待响应）：
                // 先把发送队列中的数据发完再关闭，避免丢失响应。
                sub_reactor_ptr->close_client_gracefully(ready_socket_fd);
            }
        }
    }
    
    sub_reactor_ptr->reactor_state_.store(ReactorState_Exit);
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
MainReactor::MainReactor(int sub_reactor_size, int main_events_max_size, int sub_events_max_size)
:   epfd_(-1),
    main_events_max_size_(main_events_max_size),
    sub_events_max_size_(sub_events_max_size),
    events_(nullptr),
    sub_reactor_size_(sub_reactor_size),
    reactor_state_(ReactorState_Exit)
{
    main_events_max_size_ = main_events_max_size_ < 32 ? 32 : main_events_max_size_;
    sub_events_max_size_ = sub_events_max_size_ < 32 ? 32 : sub_events_max_size_;
    events_ = new (std::nothrow) epoll_event[main_events_max_size_];
    if (events_ == nullptr) {
        LOG_ERROR("Alloc epoll_event failed.");
        main_events_max_size_ = 0;
        reactor_state_ = ReactorState_Error;
        return;
    }

    epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ == -1) {
        LOG_ERROR("epoll_create: %s", strerror(errno));
        delete []events_;
        events_ = nullptr;
        main_events_max_size_ = 0;
        reactor_state_ = ReactorState_Error;
    }
}

MainReactor::~MainReactor(void)
{
    this->stop();
}

int 
MainReactor::start(void)
{
    if (reactor_state_.load() == ReactorState_Running) {
        return 0;
    }
    if (epfd_ < 0 || events_ == nullptr) {
        LOG_ERROR("MainReactor not initialized.");
        return -1;
    }
    if (sub_reactor_size_ <= 0) {
        LOG_ERROR("SubReactor size is invalid: %d", sub_reactor_size_);
        return -1;
    }

    for (int i = 0; i < sub_reactor_size_; ++i) {
        SubReactor *sub_reactor_ptr = new (std::nothrow) SubReactor(sub_events_max_size_);
        if (sub_reactor_ptr == nullptr) {
            LOG_ERROR("Alloc SubReactor failed.");
            for (auto iter = set_sub_reactors_.begin(); iter != set_sub_reactors_.end(); ++iter) {
                delete *iter;
            }
            set_sub_reactors_.clear();
            return -1;
        }

        if (sub_reactor_ptr->start() != 0) {
            delete sub_reactor_ptr;
            for (auto iter = set_sub_reactors_.begin(); iter != set_sub_reactors_.end(); ++iter) {
                delete *iter;
            }
            set_sub_reactors_.clear();
            return -1;
        }
        set_sub_reactors_.insert(sub_reactor_ptr);
    }

    // 必须先置为运行状态，事件线程启动后会立即检查该状态
    reactor_state_ = ReactorState_Running;

    os::Task task;
    task.work_func = MainReactor::event_wait;
    task.thread_arg = this;
    task.exit_arg = this;

    int ret = ReactorManager::instance().add_task(task);
    if (ret <= 0) {
        LOG_GLOBAL_ERROR("Add main reactor task failed.");
        reactor_state_ = ReactorState_Exit;

        for (auto iter = set_sub_reactors_.begin(); iter != set_sub_reactors_.end(); ++iter) {
            delete *iter;
        }
        set_sub_reactors_.clear();
        return -1;
    }

    return 0;
}

int 
MainReactor::stop(void)
{
    ReactorState old_state = reactor_state_.load();
    if (old_state == ReactorState_Error) {
        reactor_state_ = ReactorState_Exit;
        return -1;
    }

    if (old_state != ReactorState_Exit) {
        reactor_state_ = ReactorState_WaitExit;

        // 等待监听线程退出，最多等待 5 秒，避免任务没有执行时永久阻塞
        bool exited = false;
        for (int i = 0; i < 100; ++i) {
            if (reactor_state_.load() == ReactorState_Exit) {
                exited = true;
                break;
            }
            os::Time::sleep(50);
        }

        if (exited == false) {
            // 事件线程仍在使用 acceptor/epoll，释放资源会导致 use-after-free
            LOG_GLOBAL_ERROR("MainReactor event thread did not exit in time.");
            return -1;
        }
    }

    this->cleanup();
    reactor_state_ = ReactorState_Exit;
    return 0;
}

void 
MainReactor::cleanup(void)
{
    // 移除服务端（同时关闭其客户端连接并通知 NetServer）
    while (true) {
        sock_id_t sid = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (acceptor_.empty()) {
                break;
            }
            sid = acceptor_.begin()->first;
        }
        this->remove_server_accept(sid);
    }

    // 关闭所有的sub reactor
    std::set<SubReactor*> sub_reactors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sub_reactors.swap(set_sub_reactors_);
        mp_cli_to_sub_reactor_.clear();
        mp_srv_clients_.clear();
    }
    for (auto iter = sub_reactors.begin(); iter != sub_reactors.end(); ++iter) {
        delete *iter;
    }

    // 关闭epoll, 释放资源
    if (epfd_ >= 0) {
        ::close(epfd_);
        epfd_ = -1;
    }
    if (events_ != nullptr) {
        delete []events_;
        events_ = nullptr;
    }
}

void 
MainReactor::on_client_closed(sock_id_t cid, void *arg)
{
    MainReactor *main_reactor_ptr = reinterpret_cast<MainReactor*>(arg);
    if (main_reactor_ptr == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(main_reactor_ptr->mutex_);
    main_reactor_ptr->mp_cli_to_sub_reactor_.erase(cid);
    for (auto iter = main_reactor_ptr->mp_srv_clients_.begin();
         iter != main_reactor_ptr->mp_srv_clients_.end(); ++iter) {
        iter->second.erase(cid);
    }
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

    std::lock_guard<std::mutex> lock(mutex_);
    int server_fd = handle_ptr->acceptor->get_socket();
    if (acceptor_.find(server_fd) != acceptor_.end()) {
        LOG_WARN("Server acceptor[%d] already exists.", server_fd);
        return -1;
    }

    struct epoll_event ep_events;
    memset(&ep_events, 0, sizeof(ep_events));
    ep_events.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    ep_events.data.fd = server_fd;
    LOG_INFO("Reactor add server acceptor: [socket: %d]", server_fd);
    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, server_fd, &ep_events) < 0) {
        LOG_ERROR("epoll_ctl: %s", strerror(errno));
        return -1;
    }

    acceptor_[server_fd] = handle_ptr;
    mp_srv_clients_[server_fd].clear();

    return 0;
}

int 
MainReactor::remove_server_accept(sock_id_t sid)
{
    EventHandle_t *handle_ptr = nullptr;
    std::set<sock_id_t> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto accept_iter = acceptor_.find(sid);
        if (accept_iter == acceptor_.end()) {
            LOG_WARN("Can't find server id[%ld]", static_cast<long>(sid));
            return -1;
        }

        handle_ptr = accept_iter->second;
        acceptor_.erase(accept_iter);

        auto client_iter = mp_srv_clients_.find(sid);
        if (client_iter != mp_srv_clients_.end()) {
            clients.swap(client_iter->second);
            mp_srv_clients_.erase(client_iter);
        }
    }

    // 关闭已经存在的所有客户端连接
    for (auto client_iter = clients.begin(); client_iter != clients.end(); ++client_iter) {
        this->remove_client_conn(*client_iter);
    }

    // 关闭服务端端口监听
    if (handle_ptr != nullptr) {
        if (handle_ptr->acceptor != nullptr) {
            handle_ptr->acceptor->close();
        }

        NetServer* connect_ptr = reinterpret_cast<NetServer*>(handle_ptr->server_arg);
        if (connect_ptr != nullptr) {
            try {
                connect_ptr->set_state(NetConnectState_Disconnected);
                connect_ptr->notify_server_stop_listen();
            } catch (...) {
                LOG_GLOBAL_ERROR("notify server stop listen callback threw exception.");
            }
        }
    }

    return 0;
}

int 
MainReactor::remove_client_conn(sock_id_t cid)
{
    SubReactor *sub_reactor_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto sub_reactor_iter = mp_cli_to_sub_reactor_.find(cid);
        if (sub_reactor_iter == mp_cli_to_sub_reactor_.end()) {
            LOG_GLOBAL_WARN("Can't find client id[%llx]", static_cast<unsigned long long>(cid));
            return -1;
        }

        sub_reactor_ptr = sub_reactor_iter->second;
        mp_cli_to_sub_reactor_.erase(sub_reactor_iter);
        for (auto iter = mp_srv_clients_.begin(); iter != mp_srv_clients_.end(); ++iter) {
            iter->second.erase(cid);
        }
    }

    if (sub_reactor_ptr != nullptr) {
        sub_reactor_ptr->remove_client_conn(cid);
    }

    return 0;
}

SubReactor * 
MainReactor::assign_conn_to_sub_reactor(ClientConn_t *conn)
{
    (void)conn;

    std::lock_guard<std::mutex> lock(mutex_);
    if (set_sub_reactors_.empty()) {
        LOG_GLOBAL_WARN("SubReactor size is empty!");
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
    std::lock_guard<std::mutex> lock(mutex_);
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
    while (main_reactor_ptr->reactor_state_.load() == ReactorState_Running) {
        int event_ret = ::epoll_wait(main_reactor_ptr->epfd_, main_reactor_ptr->events_,
                                     main_reactor_ptr->main_events_max_size_, 300);
        if (event_ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_GLOBAL_ERROR("epoll_wait: %s", strerror(errno));
            break;
        } else if (event_ret == 0) {
            continue;
        }

        for (int i = 0; i < event_ret; ++i) {
            int ready_socket_fd = main_reactor_ptr->events_[i].data.fd;

            sock_id_t client_id = 0;
            client_conn_func_t client_conn_func = nullptr;
            void *server_arg = nullptr;
            bool conn_established = false;

            {
                std::lock_guard<std::mutex> lock(main_reactor_ptr->mutex_);
                auto handle_iter = main_reactor_ptr->acceptor_.find(ready_socket_fd);
                if (handle_iter == main_reactor_ptr->acceptor_.end() ||
                    handle_iter->second == nullptr ||
                    handle_iter->second->acceptor == nullptr) {
                    LOG_GLOBAL_WARN("Cant find EventHandle of socket[%d]", ready_socket_fd);
                    continue;
                }

                EventHandle_t *handle_ptr = handle_iter->second;
                int server_fd = handle_ptr->acceptor->get_socket();
                int client_sock_fd = -1;
                struct sockaddr_in addr;
                socklen_t addrlen = sizeof(addr);
                memset(&addr, 0, sizeof(addr));

                // accept 在锁内执行，避免与 remove_server_accept 关闭监听 socket 竞争
                if (handle_ptr->acceptor->accept(client_sock_fd,
                                                 reinterpret_cast<sockaddr*>(&addr),
                                                 &addrlen) < 0) {
                    continue;
                }

                ClientConn_t *client_conn_ptr = new (std::nothrow) ClientConn_t;
                if (client_conn_ptr == nullptr) {
                    ::close(client_sock_fd);
                    LOG_GLOBAL_ERROR("Alloc ClientConn failed.");
                    continue;
                }

                client_conn_ptr->client_id = client_sock_fd;
                if (client_conn_ptr->socket_ptr->set_socket(client_sock_fd,
                                                            reinterpret_cast<sockaddr_in*>(&addr),
                                                            &addrlen) != 0 ||
                    client_conn_ptr->socket_ptr->setnonblocking() != 0) {
                    delete client_conn_ptr;
                    continue;
                }

                client_conn_ptr->server_arg = handle_ptr->server_arg;
                client_conn_ptr->client_func = handle_ptr->client_func;
                client_conn_ptr->connect_arg = handle_ptr->connect_arg;
                client_conn_ptr->client_conn_func = handle_ptr->client_conn_func;
                client_conn_ptr->close_func = MainReactor::on_client_closed;
                client_conn_ptr->close_arg = main_reactor_ptr;

                SubReactor* sub_reactor_ptr = nullptr;
                if (main_reactor_ptr->set_sub_reactors_.empty() == false) {
                    sub_reactor_ptr = *(main_reactor_ptr->set_sub_reactors_.begin());
                    for (auto iter = main_reactor_ptr->set_sub_reactors_.begin();
                         iter != main_reactor_ptr->set_sub_reactors_.end(); ++iter) {
                        if (sub_reactor_ptr->get_client_conn_size() > (*iter)->get_client_conn_size()) {
                            sub_reactor_ptr = *iter;
                        }
                    }
                }
                if (sub_reactor_ptr == nullptr) {
                    delete client_conn_ptr;
                    continue;
                }

                // 先登记连接关系，再注册到 sub reactor：
                // 注册后连接随时可能被事件线程关闭，close_func 需要能等到主锁并清理这些表。
                main_reactor_ptr->mp_cli_to_sub_reactor_[client_sock_fd] = sub_reactor_ptr;
                main_reactor_ptr->mp_srv_clients_[server_fd].insert(client_sock_fd);

                if (sub_reactor_ptr->add_client_conn(client_conn_ptr) != 0) {
                    main_reactor_ptr->mp_cli_to_sub_reactor_.erase(client_sock_fd);
                    main_reactor_ptr->mp_srv_clients_[server_fd].erase(client_sock_fd);
                    delete client_conn_ptr;
                    continue;
                }

                client_id = client_conn_ptr->client_id;
                client_conn_func = handle_ptr->client_conn_func;
                server_arg = handle_ptr->server_arg;
                conn_established = true;

                LOG_GLOBAL_INFO("Server add client[%s]", client_conn_ptr->socket_ptr->get_ip_info().c_str());
            }

            // 用户回调不能在持锁状态下执行
            if (conn_established == true && client_conn_func != nullptr) {
                bool still_connected = false;
                {
                    std::lock_guard<std::mutex> lock(main_reactor_ptr->mutex_);
                    still_connected = (main_reactor_ptr->mp_cli_to_sub_reactor_.find(client_id) !=
                                       main_reactor_ptr->mp_cli_to_sub_reactor_.end());
                }

                if (still_connected) {
                    try {
                        client_conn_func(client_id, server_arg);
                    } catch (...) {
                        LOG_GLOBAL_ERROR("client connect callback threw exception.");
                    }
                }
            }
        }
    }

    main_reactor_ptr->reactor_state_.store(ReactorState_Exit);
    
    return nullptr;
}

void* 
MainReactor::show_connect_info(void *arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    MainReactor *main_reactor_ptr = reinterpret_cast<MainReactor*>(arg);
    
    while (main_reactor_ptr->reactor_state_.load() == ReactorState_Running) {
        size_t acceptor_size = 0;
        std::vector<SubReactor*> sub_reactors;
        {
            std::lock_guard<std::mutex> lock(main_reactor_ptr->mutex_);
            acceptor_size = main_reactor_ptr->acceptor_.size();
            sub_reactors.assign(main_reactor_ptr->set_sub_reactors_.begin(),
                                main_reactor_ptr->set_sub_reactors_.end());
        }

        fprintf(stdout, "==========================================================\n");
        fprintf(stdout, "MainReactor: %zu\n", acceptor_size);
        int index = 0;
        for (auto iter = sub_reactors.begin(); iter != sub_reactors.end(); ++iter) {
            fprintf(stdout, "SubReactor-%d: %d\n", ++index, (*iter)->get_client_conn_size());
        }
        fprintf(stdout, "==========================================================\n");
        os::Time::sleep(2000);
    }

    return nullptr;
}

}