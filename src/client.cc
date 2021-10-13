#include "reactor.h"
#include "linux_reactor.h"

namespace reactor {

NetClient::NetClient(void)
{
    sid_ = reinterpret_cast<server_id_t>(this);
}

NetClient::~NetClient(void)
{
    disconnect();
}

int
NetClient::connect_v(void)
{
    
}

int 
NetClient::connect(const std::string &url)
{
    url_ = url;
    url_parser_.clear();
    int ret = url_parser_.parser(url);
    if (ret < 0) {
        LOG_WARN("Url Parser failed: %s: %d", url.c_str(), ret);
        return -1;
    }

    // 在断开连接时会主动释放内存
    ClientConn_t  *client_conn_ptr = new ClientConn_t;
    client_conn_ptr->client_ptr->create_socket(url_parser_.addr_, url_parser_.port_);
    if (client_conn_ptr->client_ptr->get_socket_state() == false) {
        return -1;
    }
    
    if (client_conn_ptr->client_ptr->connect() < 0) {
        LOG_WARN("Connect Failed[%s: %d]", url_parser_.addr_, url_parser_.port_);
        return -1;
    }

    handle_.exit = false;
    handle_.server_id = sid_;
    handle_.acceptor = nullptr;
    handle_.events = EventType_In | EventType_RDHup | EventType_Err;
    handle_.method = EventMethod_Epoll;
    
    handle_.client_arg = this;
    handle_.client_func = client_func;

    ret = SubReactor::instance().server_register(&handle_);
    if (ret < 0) {
        LOG_WARN("Client register Failed[%s: %d]", url_parser_.addr_, url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }

    ret = SubReactor::instance().add_client_conn(sid_, client_conn_ptr);
    if (ret < 0) {
        LOG_WARN("Add client connection Failed[%s: %d]", url_parser_.addr_, url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }
    cid_ = client_conn_ptr->client_id;
    client_conn_ptr_ = client_conn_ptr;

    return ret;
}

int 
NetClient::reconnect(void)
{
    return connect(url_);
}

int 
NetClient::disconnect(void)
{
    client_conn_ptr_ = nullptr;
    return SubReactor::instance().remove_client_conn(sid_, cid_);
}

int 
NetClient::handle_msg(ByteBuffer &buffer)
{
    return 0;
}

int 
NetClient::handle_msg(ptl::HttpPtl &ptl)
{
    return 0;
}

int 
NetClient::handle_msg(ptl::WebsocketPtl &ptl)
{
    return 0;
}

void* 
NetClient::client_func(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    NetClient *client_ptr = (NetClient*)arg;
    if (client_ptr->client_conn_ptr_ == nullptr) {
        LOG_GLOBAL_WARN("client_conn_ptr_ is nullptr[url: %s]", client_ptr->url_.c_str());
        return nullptr;
    }

    os::SocketTCP *socket_ptr = client_ptr->client_conn_ptr_->client_ptr;
    if (socket_ptr->get_socket_state() == false) {
        LOG_GLOBAL_WARN("Client socket[%s] closed.", socket_ptr->get_ip_info().c_str());
        return nullptr;
    }

    ByteBuffer buffer;
    socket_ptr->recv(buffer);
    if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Raw) {
        client_ptr->handle_msg(buffer);
    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Http) {
        ptl::HttpPtl http_ptl;
        ptl::HttpParse_ErrorCode err;
        do {
            err = http_ptl.parse(buffer);
            if (err == ptl::HttpParse_OK) {
                client_ptr->handle_msg(http_ptl);
                http_ptl.clear();
            } else if (err != ptl::HttpParse_ContentNotEnough) {
                // 协议解析错误时，断开连接
                LOG_GLOBAL_WARN("Parse client send data failed[PTL: HTTP, server: %s]", 
                        socket_ptr->get_ip_info().c_str());
                client_ptr->disconnect();
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
                // 协议解析错误时，断开连接
                LOG_GLOBAL_WARN("Parse client send data failed[PTL: Websocket, server: %s]", 
                        socket_ptr->get_ip_info().c_str());
                client_ptr->disconnect();
            }
        } while (err == ptl::WebsocketParse_OK);
    } else {
        LOG_GLOBAL_WARN("Unknown ptl: %d", client_ptr->url_parser_.type_);
    }

    return nullptr;
}

}