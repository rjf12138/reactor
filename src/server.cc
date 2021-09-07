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

    EventHandle_t handle;
    handle.tcp_conn = &server_;
    handle.type = EventType_In | EventType_RDHup | EventType_Err;
    handle.is_accept = true;
    handle.method = EventMethod_Epoll;
    handle.op = EventOperation_Add;
}

int 
Server::handle_msg(ByteBuffer &buffer)
{

}

int 
Server::handle_msg(ptl::HttpPtl &ptl)
{

}

int 
Server::handle_msg(ptl::WebsocketPtl &ptl)
{

}

void* 
Server::client_func(void* arg)
{

}
void* 
Server::accept_func(void* arg)
{

}

}