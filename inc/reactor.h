#ifndef __REACTOR_H__
#define __REACTOR_H__

#include "basic/basic_head.h"
#include "basic/byte_buffer.h"
#include "basic/logger.h"
#include "protocol/protocol.h"
#include "system/system.h"
#include "util/util.h"
#include "reactor_define.h"

namespace reactor {
///////////////////////// 客户端类 /////////////////////////
enum NetConnectState {
    NetConnectState_Disconnected,
    NetConnectState_UpgradePtl,
    NetConnectState_Connected,
    NetConnectState_Listening,
};

////////////////////// 启动和终止函数 //////////////////////////////////////////////////
extern int reactor_start(const ReactorConfig_t &config);
extern int reactor_stop(void);

//////////////////////////////////////////////////////////////////////////////////////
class NetClient : public basic::Logger, public util::MsgObject {
public:
    NetClient(void);
    virtual ~NetClient(void);
    
    // tcp协议连接到服务器， url: raw://ip:port
    int connect(const std::string &url);
    // 与服务器断开连接
    int disconnect(void);

    // 添加定时任务。成功返回：定时器id
    util::timer_id_t add_timer_task(util::TimerEvent_t &event);
    // 取消定时任务
    int cancel_timer_task(util::timer_id_t tid);

    // 获取IP信息
    std::string get_ip_info(void);
    // 获取客户端当前状态
    NetConnectState get_state(void);
    // 设置客户端当前状态
    void set_state(NetConnectState state);

    // 发送数据到服务器上
    ssize_t send_data(const ByteBuffer &buff);

    // 处理服务器发送的数据
    virtual int handle_msg(basic::ByteBuffer &buffer);
    // 对端连接断开通知
    virtual int notify_client_disconnected(client_id_t cid);
    // 进程内消息收到时回调函数
    virtual int msg_handler(util::obj_id_t sender, const basic::ByteBuffer &msg);

private:
    static void* client_func(void* arg);// arg: EventHandle_t
protected:
    std::string url_;
    ptl::URLParser url_parser_;

private:
    server_id_t sid_;
    client_id_t cid_;
    ClientConn_t *client_conn_ptr_;
    EventHandle_t handle_;

    NetConnectState state_;
};

class HttpNetClient : public NetClient {
public:
    HttpNetClient(void);
    virtual ~HttpNetClient(void);

    // http 连接到服务器， url: http://ip:port/param, content： 连接时需要发送的消息
    int connect(const std::string &url);
    // 断开与服务器的连接
    int disconnect(void);

    // 添加定时任务。成功返回：定时器id
    util::timer_id_t add_timer_task(util::TimerEvent_t &event) {return NetClient::add_timer_task(event);}
    // 取消定时任务
    int cancel_timer_task(util::timer_id_t tid) {return NetClient::cancel_timer_task(tid);}

    // 发送数据
    ssize_t send_data(ptl::HttpPtl &http_ptl);
    // 接收服务端消息
    virtual int handle_msg(ptl::HttpPtl &http_ptl);

private:
    ptl::HttpPtl http_ptl_;
};

class WSNetClient : public NetClient {
public:
    WSNetClient(bool heartbeat = false, int heartbeat_time = 30);
    virtual ~WSNetClient(void);

    // websocket 连接到服务器， url格式: ws:://ip:port/param, content: 在协议升级时需要携带的数据
    int connect(const std::string &url, basic::ByteBuffer &content);
    // 断开连接，content： 断开连接前要发送给服务器的数据
    int disconnect(basic::ByteBuffer &content);
    // 断开与服务端的连接
    int disconnect(void);

    // 添加定时任务。成功返回：定时器id
    util::timer_id_t add_timer_task(util::TimerEvent_t &event) {return NetClient::add_timer_task(event);}
    // 取消定时任务
    int cancel_timer_task(util::timer_id_t tid) {return NetClient::cancel_timer_task(tid);}

    // 发送数据
    ssize_t send_data(basic::ByteBuffer &content, int opcode, bool is_mask = false);
    // 接收服务端消息
    virtual int handle_msg(ptl::WebsocketPtl &ptl);

private:
    // 发送 websocket 协议升级请求
    int ws_upgrade_request(basic::ByteBuffer &content);
    // 处理 websocket 协议升级回复
    int handle_ws_upgrade_response(ptl::HttpPtl &ptl);

private:
    bool is_heartbeat_;
    int heartbeat_time_;
    ptl::WebsocketPtl ws_ptl_;
};

///////////////////// 服务端类 //////////////////////////////
class NetServer : public basic::Logger, public util::MsgObject {
public:
    NetServer(void);
    virtual ~NetServer(void);

    // 开启服务器
    int start(const std::string &ip, int port, ptl::ProtocolType type);
    // 停止服务器
    int stop(void);

    // 添加定时任务。成功返回：定时器id
    util::timer_id_t add_timer_task(util::TimerEvent_t &event);
    // 取消定时任务
    int cancel_timer_task(util::timer_id_t tid);

    // 设置服务器状态
    void set_state(NetConnectState state) {state_ = state;}
    // 获取服务器状态
    NetConnectState get_state() const {return state_;}
    // 获取IP信息
    std::string get_ip_info(void);
    // 关闭客户端连接, 参数 cid 可以从 handle_client_conn 中获取
    int close_client(client_id_t cid);
    // 发送数据给客户端
    ssize_t send_data(client_id_t cid, const ByteBuffer &buff);

    // 接收客户端的消息，只有 ptl::ProtocolType 指定为 ptl::ProtocolType_Raw 才会被调用
    virtual int handle_msg(client_id_t cid, ByteBuffer &buffer);
    // 接收客户端的消息，只有 ptl::ProtocolType 指定为 ptl::ProtocolType_Http 才会被调用
    virtual int handle_msg(client_id_t cid, ptl::HttpPtl &ptl);
    // 接收客户端的消息，只有 ptl::ProtocolType 指定为 ptl::ProtocolType_Websocket 才会被调用
    virtual int handle_msg(client_id_t cid, ptl::WebsocketPtl &ptl);
    // 客户端连接到服务器时会调用
    virtual int handle_client_conn(client_id_t cid);
    // 客户端连接断开时会调用
    virtual int notify_client_disconnected(client_id_t cid);
    // 服务端停止监听时的回调
    virtual int notify_server_stop_listen(void);

    // 进程内消息收到时回调函数
    virtual int msg_handler(util::obj_id_t sender, const basic::ByteBuffer &msg);

private:
    static void* client_func(void* arg); // 处理客户端发过来的数据
    static void client_conn_func(client_id_t id, void* arg); // 客户端连接时的处理函数

private:
    server_id_t id_;
    EventHandle_t handle_;

    ptl::ProtocolType type_;
    os::SocketTCP server_;

    NetConnectState state_;
};
}

#endif