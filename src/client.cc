#include "reactor.h"
#include "linux_reactor.h"

namespace reactor {

NetClient::NetClient(void)
: cid_(0),
  client_conn_ptr_(nullptr),
  state_(NetConnectState_Disconnected),
  main_reactor_ptr_(nullptr),
  sub_reactor_ptr_(nullptr)
{
}

NetClient::~NetClient(void)
{
    disconnect();
}

int 
NetClient::connect(const std::string &url)
{
    if (state_.load() != NetConnectState_Disconnected) {
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

    main_reactor_ptr_ = ReactorManager::instance().get_reactor();
    if (main_reactor_ptr_ == nullptr) {
        LOG_WARN("Reactor is not running, can not connect.");
        return -1;
    }

    // 在断开连接时会主动释放内存
    ClientConn_t *client_conn_ptr = new (std::nothrow) ClientConn_t;
    if (client_conn_ptr == nullptr) {
        LOG_ERROR("Alloc ClientConn failed.");
        return -1;
    }

    if (client_conn_ptr->socket_ptr->create_socket(url_parser_.addr_, url_parser_.port_) != 0 ||
        client_conn_ptr->socket_ptr->get_socket_state() == false) {
        LOG_WARN("Create socket failed[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }

    if (client_conn_ptr->socket_ptr->connect() < 0) {
        LOG_WARN("Connect Failed[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        delete client_conn_ptr; // 析构时会关闭 socket
        return -1;
    }

    if (client_conn_ptr->socket_ptr->setnonblocking() != 0) {
        LOG_WARN("Set socket nonblocking failed[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }

    client_conn_ptr->client_id = static_cast<sock_id_t>(client_conn_ptr->socket_ptr->get_socket());
    client_conn_ptr->client_arg = this;
    client_conn_ptr->client_func = client_func;
    client_conn_ptr->is_client = true;
    ip_info_ = client_conn_ptr->socket_ptr->get_ip_info();

    sub_reactor_ptr_ = main_reactor_ptr_->assign_conn_to_sub_reactor(client_conn_ptr);
    if (sub_reactor_ptr_ == nullptr) {
        LOG_WARN("Assign sub reactor failed[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        delete client_conn_ptr;
        return -1;
    }

    // 先保存连接信息，再注册到 reactor，避免事件线程先访问到空指针
    cid_.store(client_conn_ptr->client_id);
    client_conn_ptr_.store(client_conn_ptr);

    if (sub_reactor_ptr_->add_client_conn(client_conn_ptr) != 0) {
        LOG_WARN("Add client connection Failed[%s: %d]", url_parser_.addr_.c_str(), url_parser_.port_);
        client_conn_ptr_.store(nullptr);
        cid_.store(0);
        sub_reactor_ptr_ = nullptr;
        delete client_conn_ptr;
        return -1;
    }

    state_.store(NetConnectState_Connected);

    return 0;
}

int 
NetClient::disconnect(void)
{
    NetConnectState state = state_.load();
    if (state == NetConnectState_Disconnected) {
        client_conn_ptr_.store(nullptr);
        return 0;
    }

    SubReactor *sub_reactor_ptr = sub_reactor_ptr_;
    int ret = 0;
    if (sub_reactor_ptr != nullptr && sub_reactor_ptr->get_state() == ReactorState_Running) {
        ret = sub_reactor_ptr->remove_client_conn(cid_.load());
    }

    // 即使 remove 失败也把本地状态复位，避免状态卡死
    state_.store(NetConnectState_Disconnected);
    client_conn_ptr_.store(nullptr);

    return ret;
}

util::timer_id_t 
NetClient::add_timer_task(util::TimerEvent_t &event)
{
    return ReactorManager::instance().add_timer(event);
}

int 
NetClient::cancel_timer_task(util::timer_id_t tid)
{
    return ReactorManager::instance().cancel_timer(tid);
}

NetConnectState 
NetClient::get_state(void)
{
    return state_.load();
}

std::string 
NetClient::get_ip_info(void) 
{
    if (state_.load() == NetConnectState_Disconnected) {
        LOG_WARN("Client not connect any server.");
        return "";
    }

    return ip_info_;
}

void 
NetClient::set_state(NetConnectState state)
{
    state_.store(state);
    if (state == NetConnectState_Disconnected) {
        client_conn_ptr_.store(nullptr);
    }
}

ssize_t 
NetClient::send_data(ByteBuffer &buff)
{
    if (state_.load() == NetConnectState_Disconnected) {
        LOG_WARN("Client not connect any server.");
        return -1;
    }

    SubReactor *sub_reactor_ptr = sub_reactor_ptr_;
    if (sub_reactor_ptr == nullptr) {
        return -1;
    }

    return sub_reactor_ptr->send_data(cid_.load(), buff);
}

int 
NetClient::handle_msg(ByteBuffer &buffer)
{
    return 0;
}

int
NetClient::notify_client_disconnected(sock_id_t cid)
{
    (void)cid;
    return 0;
}

int
NetClient::check_ws_upgrade_response(ptl::HttpPtl &http_ptl)
{
    (void)http_ptl;
    return -1;
}

int 
NetClient::msg_handler(util::obj_id_t sender, basic::ByteBuffer &msg, util::topic_t topic)
{
    return 0;
}

void* 
NetClient::client_func(void* arg)
{
    if (arg == nullptr) {
        return nullptr;
    }

    ClientConn_t *connect_ptr = reinterpret_cast<ClientConn_t*>(arg);
    if (connect_ptr->socket_ptr == nullptr || connect_ptr->closed.load() == true) {
        return nullptr;
    }

    NetClient *client_ptr = reinterpret_cast<NetClient*>(connect_ptr->client_arg);
    if (client_ptr == nullptr) {
        return nullptr;
    }

    NetConnectState client_state = client_ptr->get_state();
    if (client_state != NetConnectState_Connected &&
        client_state != NetConnectState_UpgradePtl) {
        LOG_GLOBAL_WARN("Client not connect[cid: %llu]",
                        static_cast<unsigned long long>(connect_ptr->client_id));
        return nullptr;
    }

    // 注意：连接对象在回调返回前不会被释放（SubReactor 延迟删除），
    // 这里使用局部指针，避免 disconnect 把 client_conn_ptr_ 置空后继续访问。
    ByteBuffer &buffer = connect_ptr->recv_buffer;
    os::SocketTCP *socket_ptr = connect_ptr->socket_ptr;
    if (socket_ptr->get_socket_state() == false) {
        LOG_GLOBAL_WARN("Client socket[%s] closed.", socket_ptr->get_ip_info().c_str());
        return nullptr;
    }

    try {
        ssize_t size = socket_ptr->recv(buffer);
        if (size <= 0) {
            return nullptr;
        }

        if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Tcp) {
            client_ptr->mutex_.lock();
            client_ptr->handle_msg(buffer);
            client_ptr->mutex_.unlock();
            buffer.clear();
        } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Http) {
            HttpNetClient* http_client_ptr = dynamic_cast<HttpNetClient*>(client_ptr);
            if (http_client_ptr == nullptr) {
                LOG_GLOBAL_WARN("HTTP url used by NetClient, disconnect it.");
                client_ptr->disconnect();
                return nullptr;
            }

            if (connect_ptr->http_ptl == nullptr) {
                connect_ptr->http_ptl = new (std::nothrow) ptl::HttpPtl;
                if (connect_ptr->http_ptl == nullptr) {
                    LOG_GLOBAL_ERROR("Alloc HttpPtl failed.");
                    client_ptr->NetClient::disconnect();
                    return nullptr;
                }
            }
            ptl::HttpPtl &http_ptl = *(connect_ptr->http_ptl);

            ptl::HttpParse_ErrorCode err = ptl::HttpParse_ContentNotEnough;
            do {
                try {
                    err = http_ptl.parse(buffer);
                    if (err == ptl::HttpParse_OK) {
                        http_client_ptr->mutex_.lock();
                        http_client_ptr->handle_msg(http_ptl, ptl::HttpParse_OK);
                        http_client_ptr->mutex_.unlock();
                        http_ptl.clear();
                    } else if (err != ptl::HttpParse_ContentNotEnough) {
                        // 协议解析错误时，断开连接
                        LOG_GLOBAL_WARN("Parse client send data failed[PTL: HTTP, server: %s, err: %d]", 
                                socket_ptr->get_ip_info().c_str(), err);
                        http_client_ptr->mutex_.lock();
                        http_client_ptr->handle_msg(http_ptl, err);
                        http_client_ptr->mutex_.unlock();
                        http_client_ptr->disconnect();
                        break;
                    } else {
                        if (http_ptl.is_tranfer_encode()) {
                            http_client_ptr->mutex_.lock();
                            http_client_ptr->handle_msg(http_ptl, err);
                            http_client_ptr->mutex_.unlock();
                        }
                        break;
                    }
                } catch (std::runtime_error &runtime_err) {
                    // 协议解析出现异常，断开连接
                    LOG_GLOBAL_WARN("Parse Data Failed: %s]Data: \n%s\n\n%s", socket_ptr->get_ip_info().c_str(), buffer.str().c_str(), runtime_err.what());
                    http_client_ptr->disconnect();
                    break;
                } catch (...) {
                    LOG_GLOBAL_ERROR("Parse http data threw unknown exception, disconnect it.");
                    http_client_ptr->disconnect();
                    break;
                }
            } while (err == ptl::HttpParse_OK &&
                     buffer.data_size() > 0 &&
                     connect_ptr->closed.load() == false);
        } else if (client_ptr->url_parser_.type_ == ptl::ProtocolType_Websocket) {
            WSNetClient* ws_client_ptr = dynamic_cast<WSNetClient*>(client_ptr);
            if (ws_client_ptr == nullptr) {
                LOG_GLOBAL_WARN("Websocket url used by NetClient, disconnect it.");
                client_ptr->disconnect();
                return nullptr;
            }

            if (connect_ptr->http_ptl == nullptr) {
                connect_ptr->http_ptl = new (std::nothrow) ptl::HttpPtl;
            }
            if (connect_ptr->ws_ptl == nullptr) {
                connect_ptr->ws_ptl = new (std::nothrow) ptl::WebsocketPtl;
            }
            if (connect_ptr->http_ptl == nullptr || connect_ptr->ws_ptl == nullptr) {
                LOG_GLOBAL_ERROR("Alloc websocket parser failed.");
                client_ptr->NetClient::disconnect();
                return nullptr;
            }
            ptl::HttpPtl &http_ptl = *(connect_ptr->http_ptl);
            ptl::WebsocketPtl &ws_ptl = *(connect_ptr->ws_ptl);

            while (connect_ptr->closed.load() == false) {
                NetConnectState state = client_ptr->get_state();
                if (state == NetConnectState_UpgradePtl) {
                    ptl::HttpParse_ErrorCode http_err;
                    try {
                        http_err = http_ptl.parse(buffer);
                    } catch (...) {
                        LOG_GLOBAL_ERROR("Parse websocket upgrade response threw exception.");
                        client_ptr->NetClient::disconnect();
                        break;
                    }

                    if (http_err == ptl::HttpParse_OK) {
                        int ret = ws_client_ptr->check_ws_upgrade_response(http_ptl);
                        http_ptl.clear();
                        if (ret != 0) {
                            // 协议升级失败，断开连接
                            LOG_GLOBAL_WARN("Upgrade to websocket failed[PTL: HTTP, server: %s]", 
                                socket_ptr->get_ip_info().c_str());
                            client_ptr->NetClient::disconnect();
                            break;
                        }
                        ws_client_ptr->set_state(NetConnectState_Connected);
                        // 继续处理升级响应后面可能跟着的 websocket 帧
                        continue;
                    }

                    if (http_err != ptl::HttpParse_ContentNotEnough) {
                        // 协议解析错误时，断开连接
                        LOG_GLOBAL_WARN("Parse data failed[PTL: HTTP, server: %s]", 
                                socket_ptr->get_ip_info().c_str());
                        client_ptr->NetClient::disconnect();
                    }
                    break;
                }

                if (state == NetConnectState_Connected) {
                    ptl::WebsocketParse_ErrorCode ws_err;
                    try {
                        ws_err = ws_ptl.parse(buffer);
                    } catch (...) {
                        LOG_GLOBAL_ERROR("Parse websocket frame threw exception.");
                        client_ptr->NetClient::disconnect();
                        break;
                    }

                    if (ws_err == ptl::WebsocketParse_OK) {
                        ws_client_ptr->mutex_.lock();
                        ws_client_ptr->handle_msg(ws_ptl, ptl::WebsocketParse_OK);
                        ws_client_ptr->mutex_.unlock();
                        ws_ptl.clear();
                        continue;
                    }

                    if (ws_err != ptl::WebsocketParse_PacketNotEnough) {
                        // 协议解析错误时，断开连接
                        LOG_GLOBAL_WARN("Parse data failed[PTL: Websocket, server: %s]", 
                                socket_ptr->get_ip_info().c_str());
                        ws_client_ptr->mutex_.lock();
                        ws_client_ptr->handle_msg(ws_ptl, ws_err);
                        ws_client_ptr->mutex_.unlock();
                        client_ptr->NetClient::disconnect();
                    }
                    break;
                }

                break;
            }
        } else {
            LOG_GLOBAL_WARN("Unknown ptl: %d", client_ptr->url_parser_.type_);
        }
    } catch (const std::exception &e) {
        LOG_GLOBAL_ERROR("Process client data threw exception: %s", e.what());
        client_ptr->NetClient::disconnect();
    } catch (...) {
        LOG_GLOBAL_ERROR("Process client data threw unknown exception.");
        client_ptr->NetClient::disconnect();
    }

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
HttpNetClient::connect(const std::string &url)
{
    // 先校验协议类型，避免建立连接后才发现类型不匹配
    ptl::URLParser parser;
    if (parser.parser(url) != ptl::ParserError_Ok ||
        parser.type_ != ptl::ProtocolType_Http) {
        LOG_WARN("It's not a http url: %s", url.c_str());
        return -1;
    }

    int ret = NetClient::connect(url);
    if (ret < 0) {
        LOG_WARN("HTTP Client connect server failed.");
        return -1;
    }

    return 0;
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
    LOG_GLOBAL_INFO("Send data size: %d", buffer.data_size());
    return NetClient::send_data(buffer);
}

int 
HttpNetClient::handle_msg(ptl::HttpPtl &http_ptl, ptl::HttpParse_ErrorCode err)
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
    // 先校验协议类型，避免建立连接后才发现类型不匹配
    ptl::URLParser parser;
    if (parser.parser(url) != ptl::ParserError_Ok ||
        parser.type_ != ptl::ProtocolType_Websocket) {
        LOG_WARN("It's not a websocket url: %s", url.c_str());
        return -1;
    }

    int ret = NetClient::connect(url);
    if (ret < 0) {
        LOG_WARN("Websocket Client connect server failed.");
        return -1;
    }

    if (ws_upgrade_request(content) <= 0) {
        LOG_WARN("Send websocket upgrade request failed.");
        NetClient::disconnect();
        return -1;
    }

    return 0;
}

int 
WSNetClient::disconnect(basic::ByteBuffer &content)
{
    NetConnectState state = get_state();
    if (state != NetConnectState_Connected && state != NetConnectState_UpgradePtl) {
        return NetClient::disconnect();
    }

    // 连接由服务端断开，客户端只发送断开连接的请求
    // TODO: 加个定时器当服务端超时没有断开时，由客户端来断开连接
    send_data(content, ptl::WEBSOCKET_OPCODE_CONNECTION_CLOSE);
    return 0;
}

int 
WSNetClient::disconnect(void)
{
    NetConnectState state = get_state();
    if (state != NetConnectState_Connected && state != NetConnectState_UpgradePtl) {
        return NetClient::disconnect();
    }

    // 连接由服务端断开，客户端只发送断开连接的请求
    // TODO: 加个定时器当服务端超时没有断开时，由客户端来断开连接
    basic::ByteBuffer content;
    send_data(content, ptl::WEBSOCKET_OPCODE_CONNECTION_CLOSE);
    return 0;
}

ssize_t 
WSNetClient::send_data(basic::ByteBuffer &content, int8_t opcode, bool is_mask)
{
    basic::ByteBuffer buffer;
    ws_ptl_.generate(buffer, content, opcode, is_mask);
    return NetClient::send_data(buffer);
}

ssize_t
WSNetClient::ws_upgrade_request(basic::ByteBuffer &content)
{
    std::string path = url_parser_.res_path_;
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }

    ptl::HttpPtl http_ptl;
    ws_ptl_.get_upgrade_packet(http_ptl, content, path, url_parser_.addr_);

    basic::ByteBuffer buffer;
    if (http_ptl.generate(buffer) != 0) {
        return -1;
    }

    set_state(NetConnectState_UpgradePtl);
    return NetClient::send_data(buffer);
}


int
WSNetClient::handle_msg(ptl::WebsocketPtl &ptl, ptl::WebsocketParse_ErrorCode err)
{
    return 0;
}

}