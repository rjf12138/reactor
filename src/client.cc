#include "reactor.h"
#include "linux_reactor.h"

namespace reactor {

NetClient::NetClient(void)
:state_(NetConnectState_Dissconnected)
{
    sid_ = reinterpret_cast<server_id_t>(this);
}

NetClient::~NetClient(void)
{
    disconnect();
}

int 
NetClient::connect(const std::string &url)
{
    if (state_ != NetConnectState_Dissconnected) {
        LOG_WARN("Client already connect other server[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        return -1;
    }

    url_ = url;
    url_parser_.clear();
    int ret = url_parser_.parser(url);
    if (ret != ptl::ParserError_Ok) {
        LOG_WARN("Url Parser failed[%s:ret=%d]", url.c_str(), ret);
        return -1;
    }

    // 在断开连接时会主动释放内存
    ClientConn_t  *client_conn_ptr = new ClientConn_t;
    client_conn_ptr->socket_ptr->create_socket(url_parser_.addr_, url_parser_.port_);
    if (client_conn_ptr->socket_ptr->get_socket_state() == false) {
        return -1;
    }
    
    if (client_conn_ptr->socket_ptr->connect() < 0) {
        LOG_WARN("Connect Failed[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        return -1;
    }
    client_conn_ptr->client_id = client_conn_ptr->socket_ptr->get_socket();

    handle_.exit = false;
    handle_.server_id = sid_;
    handle_.acceptor = nullptr;
    handle_.events = EventType_In | EventType_RDHup | EventType_Err | EventType_ET;
    handle_.method = EventMethod_Epoll;
    
    handle_.client_arg = this;
    handle_.client_func = client_func;

    ret = SubReactor::instance().server_register(&handle_);
    if (ret < 0) {
        LOG_WARN("Client register Failed[%s: %d]", url_parser_.addr_, url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }

    client_conn_ptr->socket_ptr->setnonblocking();
    ret = SubReactor::instance().add_client_conn(sid_, client_conn_ptr);
    if (ret < 0) {
        LOG_WARN("Add client connection Failed[%s: %d]", url_parser_.addr_, url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }
    cid_ = client_conn_ptr->client_id;
    client_conn_ptr_ = client_conn_ptr;
    state_ = NetConnectState_Connected;

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
    if (state_ != NetConnectState_Dissconnected) {
        state_ = NetConnectState_Dissconnected;
        client_conn_ptr_ = nullptr;
        return SubReactor::instance().remove_client_conn(sid_, cid_);
    }

    return 0;
}

NetConnectState 
NetClient::get_state(void)
{
    return state_;
}

void 
NetClient::set_state(NetConnectState state)
{
    state_ = state;
    if (state == NetConnectState_Dissconnected) {
        client_conn_ptr_ = nullptr;
    }
}

ssize_t 
NetClient::send_data(const ByteBuffer &buff)
{
    if (state_ == NetConnectState_Dissconnected) {
        LOG_WARN("Client not connect any server.");
        return -1;
    }

    ClientConn_t* client_conn_ptr = handle_.client_conn[cid_];
    client_conn_ptr->buff_mutex.lock();
    client_conn_ptr->send_buffer += buff;
    client_conn_ptr->buff_mutex.unlock();

    SendDataCenter::instance().send_data(client_conn_ptr->client_id);

    return buff.data_size();
}

int 
NetClient::handle_msg(ByteBuffer &buffer)
{
    return 0;
}

int
NetClient::notify_client_disconnected(client_id_t cid)
{
    return 0;
}

int 
NetClient::msg_handler(util::obj_id_t sender, const basic::ByteBuffer &msg)
{
    return 0;
}

void* 
NetClient::client_func(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    int size = 0;
    NetClient *client_ptr = (NetClient*)arg;
    ByteBuffer &buffer = client_ptr->client_conn_ptr_->recv_buffer;
    os::SocketTCP *socket_ptr = client_ptr->client_conn_ptr_->socket_ptr;
    if (client_ptr->client_conn_ptr_ == nullptr) {
        LOG_GLOBAL_WARN("client_conn_ptr_ is nullptr[url: %s]", client_ptr->url_.c_str());
        goto end;
    }

    if (socket_ptr->get_socket_state() == false) {
        LOG_GLOBAL_WARN("Client socket[%s] closed.", socket_ptr->get_ip_info().c_str());
        goto end;
    }

    size = socket_ptr->recv(buffer);
    if (size <= 0) {
        goto end;
    }
    if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Raw) {
        client_ptr->handle_msg(buffer);
        buffer.clear();
    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Http) {
        ptl::HttpPtl http_ptl;
        ptl::HttpParse_ErrorCode err;
        HttpNetClient* http_client_ptr = dynamic_cast<HttpNetClient*>(client_ptr);

        do {
            err = http_ptl.parse(buffer);
            if (err == ptl::HttpParse_OK) {
                http_client_ptr->handle_msg(http_ptl);
                http_ptl.clear();
            } else if (err != ptl::HttpParse_ContentNotEnough) {
                // 协议解析错误时，断开连接
                LOG_GLOBAL_WARN("Parse client send data failed[PTL: HTTP, server: %s]", 
                        socket_ptr->get_ip_info().c_str());
                http_client_ptr->disconnect();
            }
        } while (err == ptl::HttpParse_OK);
    } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Websocket) {
        ptl::HttpPtl http_ptl;
        ptl::HttpParse_ErrorCode err;
        ptl::WebsocketPtl ws_ptl;
        WSNetClient* ws_client_ptr = dynamic_cast<WSNetClient*>(client_ptr);

        switch (client_ptr->state_)
        {
        case NetConnectState_UpgradePtl: {
            do {
                err = http_ptl.parse(buffer);
                if (err == ptl::HttpParse_OK) {
                    int ret = ws_ptl.check_upgrade_response(http_ptl);
                    if (ret == -1) {
                        // 协议升级失败，断开连接
                        LOG_GLOBAL_WARN("Upgrade to websocket failed[PTL: HTTP, server: %s]", 
                            socket_ptr->get_ip_info().c_str());
                        client_ptr->disconnect();
                    }
                    ws_client_ptr->set_state(NetConnectState_Connected);
                } else if (err != ptl::HttpParse_ContentNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse data failed[PTL: HTTP, server: %s]", 
                            socket_ptr->get_ip_info().c_str());
                    ws_client_ptr->disconnect();
                }
            } while (err == ptl::HttpParse_OK);
        } break;
        case NetConnectState_Connected: {
            ptl::WebsocketParse_ErrorCode err;
            do {
                err = ws_ptl.parse(buffer);
                if (err == ptl::WebsocketParse_OK) {
                    ws_client_ptr->handle_msg(ws_ptl);
                    ws_ptl.clear();
                } else if (err != ptl::WebsocketParse_PacketNotEnough) {
                    // 协议解析错误时，断开连接
                    LOG_GLOBAL_WARN("Parse data failed[PTL: Websocket, server: %s]", 
                            socket_ptr->get_ip_info().c_str());
                    ws_client_ptr->disconnect();
                }
            } while (err == ptl::WebsocketParse_OK);
        } break;
        default:
            break;
        }
    } else {
        LOG_GLOBAL_WARN("Unknown ptl: %d", client_ptr->url_parser_.type_);
    }
end:
    client_ptr->handle_.state = EventHandleState_Idle;

    return nullptr;
}

/////////////////////////////// HTTP Client ////////////////////////////////////////
HttpNetClient::HttpNetClient(void)
{

}

HttpNetClient::~HttpNetClient(void)
{
    disconnect();
}

int 
HttpNetClient::connect(const std::string &url, const basic::ByteBuffer &content)
{
    int ret = NetClient::connect(url);
    if (ret < 0) {
        LOG_WARN("HTTP Client connect server failed.");
        return -1;
    }

    if (url_parser_.type_ != ptl::ProtocolType_Http) {
        LOG_WARN("It's not a http url: %s", url.c_str());
        return -1;
    }

    http_ptl_.set_request(HTTP_METHOD_GET, url_parser_.res_path_);
    http_ptl_.set_header_option(HTTP_HEADER_ContentLength, std::to_string(content.data_size()));
    http_ptl_.set_content(content);

    return send_data(http_ptl_);
}

int 
HttpNetClient::disconnect(void)
{
    return NetClient::disconnect();
}

ssize_t 
HttpNetClient::send_data(ptl::HttpPtl &http_ptl)
{
    basic::ByteBuffer buffer;
    http_ptl.generate(buffer);

    return NetClient::send_data(buffer);
}

int 
HttpNetClient::handle_msg(ptl::HttpPtl &ptl)
{
    return 0;
}

///////////////////////// Websocket /////////////////////////////////
WSNetClient::WSNetClient(bool heartbeat, int heartbeat_time)
:is_heartbeat_(heartbeat)
{
    heartbeat_time_ = (heartbeat_time <= 0 ? 30 : heartbeat_time);
}

WSNetClient::~WSNetClient(void)
{

}

int 
WSNetClient::connect(const std::string &url, basic::ByteBuffer &content)
{
    int ret = NetClient::connect(url);
    if (ret < 0) {
        LOG_WARN("Websocket Client connect server failed.");
        return -1;
    }

    if (url_parser_.type_ != ptl::ProtocolType_Websocket) {
        LOG_WARN("It's not a websocket url: %s", url.c_str());
        return -1;
    }

    return ws_upgrade_request(content);
}

int 
WSNetClient::disconnect(basic::ByteBuffer &content)
{
    // 连接由服务端断开，客户端只发送断开连接的请求
    // TODO: 加个定时器当服务端超时没有断开时，由客户端来断开连接
    send_data(content, ptl::WEBSOCKET_OPCODE_CONNECTION_CLOSE);
    return 0;
}

int 
WSNetClient::disconnect(void)
{
    // 连接由服务端断开，客户端只发送断开连接的请求
    // TODO: 加个定时器当服务端超时没有断开时，由客户端来断开连接
    basic::ByteBuffer content;
    send_data(content, ptl::WEBSOCKET_OPCODE_CONNECTION_CLOSE);
    return 0;
}

ssize_t 
WSNetClient::send_data(basic::ByteBuffer &content, int opcode, bool is_mask)
{
    basic::ByteBuffer buffer;
    ws_ptl_.generate(buffer, content, opcode, is_mask);
    return NetClient::send_data(buffer);
}

int
WSNetClient::ws_upgrade_request(basic::ByteBuffer &content)
{
    ptl::HttpPtl http_ptl;
    ws_ptl_.get_upgrade_packet(http_ptl, content, url_parser_.res_path_);

    basic::ByteBuffer buffer;
    http_ptl.generate(buffer);

    set_state(NetConnectState_UpgradePtl);
    return NetClient::send_data(buffer);
}


int
WSNetClient::handle_msg(ptl::WebsocketPtl &ptl)
{
    return 0;
}

}