#include "server.h"


namespace reactor {

Server::Server(void)
{
    id_ = reinterpret_cast<server_id_t>(this);
}

Server::~Server(void)
{

}

int 
Server::start(const std::string &ip, int port, ptl::ProtocolType type)
{
    int ret = server_.create_socket(ip, port);
    if (ret < 0) {
        return -1;
    }
    server_.listen();

    type_ = type;

    handle_.server_id = id_;
    handle_.acceptor = &server_;
    handle_.events = EventType_In | EventType_RDHup | EventType_Err;
    handle_.method = EventMethod_Epoll;
    handle_.op = EventOperation_Add;
    
    handle_.client_arg = this;
    handle_.client_func = client_func;
    handle_.client_conn_func = client_conn_func;
}


int 
Server::close_client(client_id_t id)
{

}

ssize_t 
Server::send_data(client_id_t id, const ByteBuffer &buff)
{
    ClientConn_t* client_ptr = reinterpret_cast<ClientConn_t*>(id);
    client_ptr->buff_mutex.lock();
    client_ptr->send_buffer += buff;
    client_ptr->buff_mutex.unlock();

    return buff.data_size();
}

int 
Server::handle_msg(client_id_t id, ByteBuffer &buffer)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
Server::handle_msg(client_id_t id, ptl::HttpPtl &ptl)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
Server::handle_msg(client_id_t id, ptl::WebsocketPtl &ptl)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
Server::handle_client_conn(client_id_t id)
{
    // 如果在客户端连接时需要处理一些事务，可以重载这个函数
    return 0;
}

void* 
Server::client_func(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    Server *client_ptr = (Server*)arg;
    ByteBuffer buffer;

    int ready_client_sock = 0;
    while (true) {
        client_ptr->handle_.ready_sock_mutex.lock();
        int ret = client_ptr->handle_.ready_sock.pop(ready_client_sock);
        client_ptr->handle_.ready_sock_mutex.unlock();
        if (ret < 0) {
            break;
        }

        auto find_iter = client_ptr->handle_.client_conn.find(ready_client_sock);
        if (find_iter == client_ptr->handle_.client_conn.end()) {
            LOG_GLOBAL_WARN("Can't find client socket(%d)", ready_client_sock);
            continue;
        }

        client_id_t id = client_ptr->handle_.client_conn[ready_client_sock]->client_id;
        client_ptr->handle_.client_conn[ready_client_sock]->client_ptr->recv(buffer);
        if (client_ptr->type_ == ptl::ProtocolType_Raw) {
            client_ptr->handle_msg(id, buffer);
        } else if (client_ptr->type_ == ptl::ProtocolType_Http) {
            ptl::HttpPtl http_ptl;
            ptl::HttpParse_ErrorCode err;
            do {
                err = http_ptl.parse(buffer);
                if (err == ptl::HttpParse_OK) {
                    client_ptr->handle_msg(id, http_ptl);
                    http_ptl.clear();
                } else if (err != ptl::HttpParse_ContentNotEnough) {
                    //TODO： 协议解析错误时，断开连接
                    buffer.clear();
                }
            } while (err == ptl::HttpParse_OK);
        } else if (client_ptr->type_ == ptl::ProtocolType_Websocket) {
            ptl::WebsocketPtl ws_ptl;
            ptl::WebsocketParse_ErrorCode err;
            do {
                err = ws_ptl.parse(buffer);
                if (err == ptl::WebsocketParse_OK) {
                    client_ptr->handle_msg(id, ws_ptl);
                    ws_ptl.clear();
                } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                    //TODO： 协议解析错误时，断开连接
                    buffer.clear();
                }
            } while (err == ptl::WebsocketParse_OK);
        } else {
            LOG_GLOBAL_WARN("Unknown ptl: %d", client_ptr->type_);
        }
    }

    return nullptr;
}

void 
Server::client_conn_func(client_id_t id, void* arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_WARN("arg is nullptr");
        return;
    }

    Server* server_ptr = reinterpret_cast<Server*>(arg);
    server_ptr->handle_client_conn(id);

    return;
}

}