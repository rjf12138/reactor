#include "reactor.h"
#include "linux_reactor.h"

namespace reactor {

NetServer::NetServer(void)
{
    id_ = reinterpret_cast<server_id_t>(this);
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
    
    if (server_.listen() >= 0 &&
        server_.setnonblocking() >= 0 &&
        server_.set_reuse_addr() >= 0 && 
        ReactorManager::instance().get_main_reactor_state() == ReactorState_Running) {
            type_ = type;
            handle_.exit = false;
            handle_.server_id = id_;
            handle_.acceptor = &server_;
            handle_.events = EventType_In | EventType_RDHup | EventType_Err /*| EventType_ET*/;
            handle_.method = EventMethod_Epoll;
            
            handle_.client_arg = this;
            handle_.client_func = client_func;
            handle_.client_conn_func = client_conn_func;

            ret = ReactorManager::instance().get_main_reactor()->add_server_accept(&handle_);
            if (ret >= 0) {
                state_ = NetConnectState_Listening;
                return ret;
            }
    } else {
        LOG_WARN("An Error occur when adding a server![MainReactor: %d]", ReactorManager::instance().get_main_reactor_state());
    }
    return -1;
}

int 
NetServer::stop(void)
{
    if (state_ == NetConnectState_Listening && ReactorManager::instance().get_main_reactor_state() == ReactorState_Running) {
        return ReactorManager::instance().get_main_reactor()->remove_server_accept(id_);
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
NetServer::close_client(client_id_t cid)
{
    if (state_ != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return -1;
    }
    return ReactorManager::instance().get_main_reactor()->remove_client_conn(id_, cid);
}

ssize_t 
NetServer::send_data(client_id_t id, const ByteBuffer &buff)
{
    if (state_ != NetConnectState_Listening) {
        LOG_WARN("Server not start listening!");
        return -1;
    }

    ClientConn_t* client_conn_ptr = handle_.client_conn[id];
    client_conn_ptr->buff_mutex.lock();
    client_conn_ptr->send_buffer += buff;
    client_conn_ptr->buff_mutex.unlock();

    ReactorManager::instance().get_send_datacenter()->send_data(client_conn_ptr->client_id);

    return buff.data_size();
}

int 
NetServer::handle_msg(client_id_t cid, ByteBuffer &buffer)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
NetServer::handle_msg(client_id_t cid, ptl::HttpPtl &ptl, ptl::HttpParse_ErrorCode err)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
NetServer::handle_msg(client_id_t cid, ptl::WebsocketPtl &ptl, ptl::WebsocketParse_ErrorCode err)
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
NetServer::handle_client_conn(client_id_t cid)
{
    // 如果在客户端连接时需要处理一些事务，可以重载这个函数
    return 0;
}

int 
NetServer::notify_client_disconnected(client_id_t cid)
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

    NetServer *server_ptr = reinterpret_cast<NetServer*>(arg);
    int ready_client_sock = 0;
    while (server_ptr->state_ == NetConnectState_Listening) {
        server_ptr->handle_.ready_sock_mutex.lock();
        int ret = server_ptr->handle_.ready_sock.pop(ready_client_sock);
        if (ret <= 0) {
            server_ptr->handle_.state = EventHandleState_Idle;
            server_ptr->handle_.ready_sock_mutex.unlock();
            break;
        }
        server_ptr->handle_.ready_sock_mutex.unlock();

        auto find_iter = server_ptr->handle_.client_conn.find(ready_client_sock);
        if (find_iter == server_ptr->handle_.client_conn.end()) {
            LOG_GLOBAL_WARN("Can't find client socket(%d)", ready_client_sock);
            continue;
        }

        ByteBuffer &buffer = server_ptr->handle_.client_conn[ready_client_sock]->recv_buffer;
        client_id_t id = server_ptr->handle_.client_conn[ready_client_sock]->client_id;
        ssize_t size = server_ptr->handle_.client_conn[ready_client_sock]->socket_ptr->recv(buffer);
        if (size <= 0) {
            continue;
        }

        if (server_ptr->type_ == ptl::ProtocolType_Tcp) {
            server_ptr->handle_msg(id, buffer);
            buffer.clear();
        } else if (server_ptr->type_ == ptl::ProtocolType_Http) {
            ptl::HttpParse_ErrorCode err;
            do {
                LOG_GLOBAL_DEBUG("http:\n%s", buffer.str().c_str());
                err = server_ptr->http_ptl_.parse(buffer);
                if (err == ptl::HttpParse_OK) {
                    server_ptr->handle_msg(id, server_ptr->http_ptl_, ptl::HttpParse_OK);
                    server_ptr->http_ptl_.clear();
                } else if (err != ptl::HttpParse_ContentNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse client send data failed[PTL: HTTP, client: %s]", 
                            server_ptr->handle_.client_conn[ready_client_sock]->socket_ptr->get_ip_info().c_str());
                    server_ptr->handle_msg(id, server_ptr->http_ptl_, err);
                    server_ptr->close_client(id);
                } else {
                    if (server_ptr->http_ptl_.is_tranfer_encode()) {
                        server_ptr->handle_msg(id, server_ptr->http_ptl_, err);
                    }
                }
            } while (err == ptl::HttpParse_OK);
        } else if (server_ptr->type_ == ptl::ProtocolType_Websocket) {
            ptl::WebsocketPtl ws_ptl;
            ptl::WebsocketParse_ErrorCode err;
            do {
                err = ws_ptl.parse(buffer);
                if (err == ptl::WebsocketParse_OK) {
                    server_ptr->handle_msg(id, ws_ptl, ptl::WebsocketParse_OK);
                    ws_ptl.clear();
                } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse client send data failed[PTL: WebSocket, client: %s]", 
                            server_ptr->handle_.client_conn[ready_client_sock]->socket_ptr->get_ip_info().c_str());
                    server_ptr->handle_msg(id, ws_ptl, err);
                    server_ptr->close_client(id);
                }
            } while (err == ptl::WebsocketParse_OK);
        } else {
            LOG_GLOBAL_WARN("[PTL: Unknown, client: %s]", server_ptr->handle_.client_conn[ready_client_sock]->socket_ptr->get_ip_info().c_str());
            server_ptr->close_client(id);
        }
    }
    return nullptr;
}

void 
NetServer::client_conn_func(client_id_t id, void* arg)
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