#include "reactor.h"
#include "linux_reactor.h"

namespace reactor {

NetServer::NetServer(void)
{
    state_ = NetConnectState_Disconnected;
}

NetServer::~NetServer(void)
{
    stop();
}

int 
NetServer::start(const std::string &ip, uint16_t port, ptl::ProtocolType type)
{
    int ret = server_.create_socket(ip, port);
    if (ret < 0) {
        return -1;
    }
    

    main_reactor_ptr_ = ReactorManager::instance().get_reactor();
    if (server_.listen() >= 0 &&
        server_.setnonblocking() >= 0 &&
        server_.set_reuse_addr() >= 0 && 
        main_reactor_ptr_->get_state() == ReactorState_Running) {
            type_ = type;
            handle_.acceptor = &server_;
            handle_.mutex = &mutex_;
            handle_.server_arg = this;
            handle_.client_func = client_func;
            handle_.client_conn_func = client_conn_func;

            ret = main_reactor_ptr_->add_server_accept(&handle_);
            if (ret >= 0) {
                state_ = NetConnectState_Listening;
                return ret;
            }
    } else {
        LOG_WARN("An Error occur when adding a server![MainReactor: %d]", main_reactor_ptr_->get_state());
    }
    return -1;
}

int 
NetServer::stop(void)
{
    if (state_ == NetConnectState_Listening && main_reactor_ptr_->get_state() == ReactorState_Running) {
        return main_reactor_ptr_->remove_server_accept(server_.get_socket());
    }
    state_ = NetConnectState_Disconnected;
    return 0;
}

util::timer_id_t 
NetServer::add_timer_task(util::TimerEvent_t &event)
{
    if (state_ != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return INVAILD_TIMER_ID;
    }
    return ReactorManager::instance().add_timer(event);
}

int 
NetServer::cancel_timer_task(util::timer_id_t tid)
{
    if (state_ != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return INVAILD_TIMER_ID;
    }
    return ReactorManager::instance().cancel_timer(tid);
}

std::string 
NetServer::get_ip_info(void)
{
    if (state_ != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return "";
    }
    return server_.get_ip_info();
}

int 
NetServer::close_client(sock_id_t cid)
{
    if (state_ != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return -1;
    }
    return main_reactor_ptr_->remove_client_conn(cid);
}

//发送数据
ssize_t 
NetServer::send_data(sock_id_t cid, ByteBuffer &buff)
{
    if (state_ != NetConnectState_Listening) {
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
    NetServer *server_ptr = reinterpret_cast<NetServer*>(conn_ptr->server_arg);
    int ready_client_sock = 0;
    if (server_ptr->state_ == NetConnectState_Listening) {
        ssize_t size = conn_ptr->socket_ptr->recv(conn_ptr->recv_buffer);
        if (size <= 0) {
            return nullptr;
        }

        int id = conn_ptr->socket_ptr->get_socket();
        if (server_ptr->type_ == ptl::ProtocolType_Tcp) {
            server_ptr->mutex_.lock();
            server_ptr->handle_msg(id, conn_ptr->recv_buffer);
            server_ptr->mutex_.unlock();
            conn_ptr->recv_buffer.clear();
        } else if (server_ptr->type_ == ptl::ProtocolType_Http) {
            ptl::HttpParse_ErrorCode err;
            ptl::HttpPtl http_ptl;
            do {
                //LOG_GLOBAL_DEBUG("http: %d\n%s", conn_ptr->recv_buffer.data_size(), conn_ptr->recv_buffer.str().c_str());
                err = http_ptl.parse(conn_ptr->recv_buffer);
                if (err == ptl::HttpParse_OK) {
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, http_ptl, ptl::HttpParse_OK);
                    server_ptr->mutex_.unlock();
                    LOG_GLOBAL_INFO("Parse client send data success[PTL: HTTP, client: %s]", 
                            conn_ptr->socket_ptr->get_ip_info().c_str());
                } else if (err != ptl::HttpParse_ContentNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse client send data failed[PTL: HTTP, client: %s]", 
                            conn_ptr->socket_ptr->get_ip_info().c_str());
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, http_ptl, err);
                    server_ptr->mutex_.unlock();
                    server_ptr->close_client(id); // http数据处理完成后关闭连接
                } else {
                    if (http_ptl.is_tranfer_encode()) {
                        server_ptr->mutex_.lock();
                        server_ptr->handle_msg(id, http_ptl, err);
                        server_ptr->mutex_.unlock();
                    }
                }
            } while (err == ptl::HttpParse_OK);
        } else if (server_ptr->type_ == ptl::ProtocolType_Websocket) {
            ptl::WebsocketPtl ws_ptl;
            ptl::WebsocketParse_ErrorCode err;
            do {
                err = ws_ptl.parse(conn_ptr->recv_buffer);
                if (err == ptl::WebsocketParse_OK) {
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, ws_ptl, ptl::WebsocketParse_OK);
                    server_ptr->mutex_.unlock();
                    ws_ptl.clear();
                } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse client send data failed[PTL: WebSocket, client: %s]", 
                            conn_ptr->socket_ptr->get_ip_info().c_str());
                    server_ptr->mutex_.lock();
                    server_ptr->handle_msg(id, ws_ptl, err);
                    server_ptr->mutex_.unlock();
                    server_ptr->close_client(id);
                }
            } while (err == ptl::WebsocketParse_OK);
        } else {
            LOG_GLOBAL_WARN("[PTL: Unknown, client: %s]", conn_ptr->socket_ptr->get_ip_info().c_str());
            server_ptr->close_client(id);
        }
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