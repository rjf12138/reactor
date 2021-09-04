#include "client.h"
#include "url_parser.h"

namespace reactor {

Client::Client(EventMethod method)
:method_(method)
{

}

Client::~Client(void)
{

}

int 
Client::connect(const std::string &url)
{
    url_parser_.clear();
    int ret = url_parser_.parser(url);
    if (ret < 0) {
        LOG_WARN("Url Parser failed: %s: %d", url.c_str(), ret);
        return -1;
    }

    if (method_ == EventMethod_Epoll) {
        socket_.create_socket(url_parser_.addr_, url_parser_.port_);
        if (socket_.get_socket_state() == false) {
            return -1;
        }
        socket_.connect();
        socket_.setnonblocking();
    } else {
        LOG_WARN("Unknown method: %d", method_);
        return -1;
    }

    return 0;
}

int 
Client::reconnect(void)
{
    if (method_ == EventMethod_Epoll) {
        socket_.create_socket(url_parser_.addr_, url_parser_.port_);
        if (socket_.get_socket_state() == false) {
            return -1;
        }
        socket_.connect();
        socket_.setnonblocking();
    } else {
        LOG_WARN("Unknown method: %d", method_);
        return -1;
    }

    return 0;
}

int 
Client::disconnect(void)
{
    return socket_.close();
}

int 
Client::handle_msg(ByteBuffer &buffer, ByteBuffer &send_buf, bool &is_send)
{
    return 0;
}

int 
Client::handle_msg(ptl::HttpPtl &ptl, ByteBuffer &send_buf, bool &is_send)
{
    return 0;
}

int 
Client::handle_msg(ptl::WebsocketPtl &ptl, ByteBuffer &send_buf, bool &is_send)
{
    return 0;
}

void* 
Client::client_func(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    Client *client_ptr = (Client*)arg;
    if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Raw) {
        ByteBuffer buffer;
        client_ptr->socket_.recv(buffer, );
    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Http) {

    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Websocket) {

    } else {

    }

    return nullptr;
}

}