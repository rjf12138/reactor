#include "server.h"


namespace reactor {

Server::Server(void)
{

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

    handle_.acceptor = &server_;
    handle_.events = EventType_In | EventType_RDHup | EventType_Err;
    handle_.method = EventMethod_Epoll;
    handle_.op = EventOperation_Add;
}

int 
Server::handle_msg(ByteBuffer &buffer)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
Server::handle_msg(ptl::HttpPtl &ptl)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
    return 0;
}

int 
Server::handle_msg(ptl::WebsocketPtl &ptl)
{
    // 修改数据时，需要上锁（存在多个线程修改同个变量的可能性）
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
    client_ptr->recv(buffer);
    if (client_ptr->type_ == ptl::ProtocolType_Raw) {
        client_ptr->handle_msg(buffer);
    } else if (client_ptr->type_ == ptl::ProtocolType_Http) {
        ptl::HttpPtl http_ptl;
        ptl::HttpParse_ErrorCode err;
        do {
            err = http_ptl.parse(buffer);
            if (err == ptl::HttpParse_OK) {
                client_ptr->handle_msg(http_ptl);
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
                client_ptr->handle_msg(ws_ptl);
                ws_ptl.clear();
            } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                //TODO： 协议解析错误时，断开连接
                buffer.clear();
            }
        } while (err == ptl::WebsocketParse_OK);
    } else {
        LOG_GLOBAL_WARN("Unknown ptl: %d", client_ptr->type_);
    }

    return nullptr;
}

}