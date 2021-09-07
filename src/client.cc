#include "client.h"
#include "url_parser.h"

namespace reactor {

Client::Client(EventMethod method)
{

}

Client::~Client(void)
{

}

int
Client::connect_v(void)
{
    socket_.create_socket(url_parser_.addr_, url_parser_.port_);
    if (socket_.get_socket_state() == false) {
        return -1;
    }
    socket_.connect();

    return 0;
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

    return connect_v();
}

int 
Client::reconnect(void)
{
    return connect_v();
}

int 
Client::disconnect(void)
{
    return socket_.close();
}

int 
Client::handle_msg(ByteBuffer &buffer)
{
    return 0;
}

int 
Client::handle_msg(ptl::HttpPtl &ptl)
{
    return 0;
}

int 
Client::handle_msg(ptl::WebsocketPtl &ptl)
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
    ByteBuffer buffer;
    client_ptr->socket_.recv(buffer);
    if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Raw) {
        client_ptr->handle_msg(buffer);
    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Http) {
        ptl::HttpPtl http_ptl;
        ptl::HttpParse_ErrorCode err;
        do {
            err = http_ptl.parser(buffer);
            if (err == ptl::HttpParse_OK) {
                client_ptr->handle_msg(http_ptl);
                http_ptl.clear();
            } else if (err != ptl::HttpParse_ContentNotEnough) {
                buffer.clear();
            }
        } while (err == ptl::HttpParse_OK);
    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Websocket) {
        ptl::WebsocketPtl ws_ptl;
        ptl::WebsocketParse_ErrorCode err;
        do {
            err = ws_ptl.parse(buffer);
            if (err == ptl::WebsocketParse_OK) {
                client_ptr->handle_msg(ws_ptl);
                ws_ptl.clear();
            } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                buffer.clear();
            }
        } while (err == ptl::WebsocketParse_OK);
    } else {
        LOG_GLOBAL_WARN("Unknown ptl: %d", client_ptr->url_parser_.type_);
    }

    return nullptr;
}

}