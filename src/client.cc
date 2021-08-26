#include "client.h"

namespace reactor {

Client::Client(void)
{

}

Client::~Client(void)
{

}

int 
Client::connect(const std::string &ip, int port, ptl::ProtocolType type , bool auto_reconnect)
{
    util::SocketTCP socket = 
}

int 
Client::disconnect(void)
{

}

int 
Client::handle_msg(ByteBuffer &buffer, ByteBuffer &send_buf, bool &is_send)
{

}

int 
Client::handle_msg(ptl::HttpPtl &ptl, ByteBuffer &send_buf, bool &is_send)
{

}

int 
Client::handle_msg(ptl::WebsocketPtl &ptl, ByteBuffer &send_buf, bool &is_send)
{

}

void* 
Client::client_func(void* arg)
{

}

}