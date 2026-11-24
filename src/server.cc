#include "reactor.h"
#include "linux_reactor.h"

namespace reactor {

NetServer::NetServer(void)
: type_(ptl::ProtocolType_Tcp),
  state_(NetConnectState_Disconnected),
  main_reactor_ptr_(nullptr)
{
}

NetServer::~NetServer(void)
{
    stop();
}

int 
NetServer::start(const std::string &ip, uint16_t port, ptl::ProtocolType type)
{
    if (state_.load() == NetConnectState_Listening) {
        LOG_WARN("Server is already listening.");
        return -1;
    }

    main_reactor_ptr_ = ReactorManager::instance().get_reactor();
    if (main_reactor_ptr_ == nullptr) {
        LOG_WARN("Reactor is not running, start server failed.");
        return -1;
    }

    int ret = server_.create_socket(ip, port);
    if (ret < 0) {
        return -1;
    }

    if (server_.setnonblocking() < 0 ||
        server_.set_reuse_addr() < 0 ||
        server_.listen() < 0 ||
        main_reactor_ptr_->get_state() != ReactorState_Running) {
        LOG_WARN("An Error occur when adding a server![MainReactor: %d]",
                 static_cast<int>(main_reactor_ptr_->get_state()));
        server_.close();
        return -1;
    }

    type_ = type;
    handle_.acceptor = &server_;
    handle_.mutex = &mutex_;
    handle_.server_arg = this;
    handle_.client_func = client_func;
    handle_.client_conn_func = client_conn_func;

    ret = main_reactor_ptr_->add_server_accept(&handle_);
    if (ret < 0) {
        server_.close();
        return -1;
    }

    state_ = NetConnectState_Listening;
    return 0;
}

int 
NetServer::stop(void)
{
    if (state_.load() == NetConnectState_Listening &&
        main_reactor_ptr_ != nullptr &&
        main_reactor_ptr_->get_state() == ReactorState_Running) {
        return main_reactor_ptr_->remove_server_accept(server_.get_socket());
    }

    state_ = NetConnectState_Disconnected;
    return 0;
}

util::timer_id_t 
NetServer::add_timer_task(util::TimerEvent_t &event)
{
    if (state_.load() != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return INVAILD_TIMER_ID;
    }
    return ReactorManager::instance().add_timer(event);
}

int 
NetServer::cancel_timer_task(util::timer_id_t tid)
{
    if (state_.load() != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return INVAILD_TIMER_ID;
    }
    return ReactorManager::instance().cancel_timer(tid);
}

std::string 
NetServer::get_ip_info(void)
{
    if (state_.load() != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return "";
    }
    return server_.get_ip_info();
}

int 
NetServer::close_client(sock_id_t cid)
{
    if (state_.load() != NetConnectState_Listening ||
        main_reactor_ptr_ == nullptr) {
        LOG_WARN("Server not start listening!");
        return -1;
    }

    SubReactor* sub_reactor_ptr = main_reactor_ptr_->get_client_sub_reactor(cid);
    if (sub_reactor_ptr == nullptr) {
        return -1;
    }

    // 先把发送队列中的数据发完再关闭，避免响应数据被丢弃
    return sub_reactor_ptr->close_client_gracefully(cid);
}

//发送数据
ssize_t 
NetServer::send_data(sock_id_t cid, ByteBuffer &buff)
{
    if (state_.load() != NetConnectState_Listening ||
        main_reactor_ptr_ == nullptr) {
        LOG_WARN("Server not start listening!");
        return -1;
    }

    SubReactor* sub_reactor_ptr = main_reactor_ptr_->get_client_sub_reactor(cid);
    if (sub_reactor_ptr == nullptr) {
        return -1;
    }

    return sub_reactor_ptr->send_data(cid, buff);
}

int 
NetServer::handle_msg(sock_id_t cid, ByteBuffer &buffer)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
NetServer::handle_msg(sock_id_t cid, ptl::HttpPtl &ptl, ptl::HttpParse_ErrorCode err)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
NetServer::handle_msg(sock_id_t cid, ptl::WebsocketPtl &ptl, ptl::WebsocketParse_ErrorCode err)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
NetServer::msg_handler(util::obj_id_t sender, basic::ByteBuffer &msg, util::topic_t topic)
{
    return 0;
}

int 
NetServer::handle_client_conn(sock_id_t cid)
{
    // 如果在客户端连接时需要处理一些事务，可以重载这个函数
    return 0;
}

int 
NetServer::notify_client_disconnected(sock_id_t cid)
{
    // 如果在客户端连接断开时需要处理一些事务，可以重载这个函数
    return 0;
}

int 
NetServer::notify_server_stop_listen(void)
{
    // 服务器停止监听
    return 0;
}

void* 
NetServer::client_func(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    ClientConn_t* conn_ptr = reinterpret_cast<ClientConn_t*>(arg);
    if (conn_ptr->socket_ptr == nullptr || conn_ptr->closed.load() == true) {
        return nullptr;
    }

    NetServer *server_ptr = reinterpret_cast<NetServer*>(conn_ptr->server_arg);
    if (server_ptr == nullptr ||
        server_ptr->state_.load() != NetConnectState_Listening ||
        conn_ptr->socket_ptr->get_socket_state() == false) {
        return nullptr;
    }

    try {
        ssize_t size = conn_ptr->socket_ptr->recv(conn_ptr->recv_buffer);
        if (size <= 0) {
            return nullptr;
        }

        sock_id_t id = static_cast<sock_id_t>(conn_ptr->socket_ptr->get_socket());
        if (server_ptr->type_ == ptl::ProtocolType_Tcp) {
            server_ptr->mutex_.lock();
            server_ptr->handle_msg(id, conn_ptr->recv_buffer);
            server_ptr->mutex_.unlock();
            conn_ptr->recv_buffer.clear();
        } else if (server_ptr->type_ == ptl::ProtocolType_Http) {
            if (conn_ptr->http_ptl == nullptr) {
                conn_ptr->http_ptl = new (std::nothrow) ptl::HttpPtl;
                if (conn_ptr->http_ptl == nullptr) {
                    LOG_GLOBAL_ERROR("Alloc HttpPtl failed.");
                    conn_ptr->socket_ptr->close();
                    return nullptr;
                }
            }
            ptl::HttpPtl &http_ptl = *(conn_ptr->http_ptl);
            ptl::HttpParse_ErrorCode err = ptl::HttpParse_ContentNotEnough;
            do {
                err = http_ptl.parse(conn_ptr->recv_buffer);
                if (err == ptl::HttpParse_OK) {
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, http_ptl, ptl::HttpParse_OK);
                    server_ptr->mutex_.unlock();
                    LOG_GLOBAL_INFO("Parse client send data success[PTL: HTTP, client: %s]", 
                            conn_ptr->socket_ptr->get_ip_info().c_str());
                    // 用户回调中可能调用了 close_client，停止继续处理
                    if (conn_ptr->closed.load() == true) {
                        break;
                    }
                } else if (err != ptl::HttpParse_ContentNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse client send data failed[PTL: HTTP, client: %s]", 
                            conn_ptr->socket_ptr->get_ip_info().c_str());
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, http_ptl, err);
                    server_ptr->mutex_.unlock();
                    server_ptr->close_client(id); // http数据处理完成后关闭连接
                    break;
                } else {
                    if (http_ptl.is_tranfer_encode()) {
                        server_ptr->mutex_.lock();
                        server_ptr->handle_msg(id, http_ptl, err);
                        server_ptr->mutex_.unlock();
                    }
                    break;
                }
            } while (err == ptl::HttpParse_OK &&
                     conn_ptr->recv_buffer.data_size() > 0 &&
                     conn_ptr->closed.load() == false);
        } else if (server_ptr->type_ == ptl::ProtocolType_Websocket) {
            if (conn_ptr->ws_ptl == nullptr) {
                conn_ptr->ws_ptl = new (std::nothrow) ptl::WebsocketPtl;
                if (conn_ptr->ws_ptl == nullptr) {
                    LOG_GLOBAL_ERROR("Alloc WebsocketPtl failed.");
                    conn_ptr->socket_ptr->close();
                    return nullptr;
                }
            }
            ptl::WebsocketPtl &ws_ptl = *(conn_ptr->ws_ptl);
            ptl::WebsocketParse_ErrorCode err = ptl::WebsocketParse_PacketNotEnough;
            do {
                err = ws_ptl.parse(conn_ptr->recv_buffer);
                if (err == ptl::WebsocketParse_OK) {
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, ws_ptl, ptl::WebsocketParse_OK);
                    server_ptr->mutex_.unlock();
                    ws_ptl.clear();
                    if (conn_ptr->closed.load() == true) {
                        break;
                    }
                } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse client send data failed[PTL: WebSocket, client: %s]", 
                            conn_ptr->socket_ptr->get_ip_info().c_str());
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, ws_ptl, err);
                    server_ptr->mutex_.unlock();
                    server_ptr->close_client(id);
                    break;
                }
            } while (err == ptl::WebsocketParse_OK &&
                     conn_ptr->recv_buffer.data_size() > 0 &&
                     conn_ptr->closed.load() == false);
        } else {
            LOG_GLOBAL_WARN("[PTL: Unknown, client: %s]", conn_ptr->socket_ptr->get_ip_info().c_str());
            server_ptr->close_client(id);
        }
    } catch (const std::exception &e) {
        LOG_GLOBAL_ERROR("Process server client data threw exception: %s", e.what());
        conn_ptr->socket_ptr->close();
    } catch (...) {
        LOG_GLOBAL_ERROR("Process server client data threw unknown exception.");
        conn_ptr->socket_ptr->close();
    }

    return nullptr;
}

void 
NetServer::client_conn_func(sock_id_t id, void* arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_WARN("arg is nullptr");
        return;
    }

    NetServer* server_ptr = reinterpret_cast<NetServer*>(arg);
    server_ptr->handle_client_conn(id);

    return;
}

}