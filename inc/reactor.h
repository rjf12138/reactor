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
class NetClient : public basic::Logger{
public:
    NetClient(void);
    virtual ~NetClient(void);
    
    int connect(const std::string &url);
    int reconnect(void);
    int disconnect(void);

    virtual int handle_msg(basic::ByteBuffer &buffer);
    virtual int handle_msg(ptl::HttpPtl &ptl);
    virtual int handle_msg(ptl::WebsocketPtl &ptl);

private:
    int connect_v();
    static void* client_func(void* arg);// arg: EventHandle_t

private:
    os::SocketTCP socket_;
    ptl::URLParser url_parser_;
};

///////////////////// 服务端类 //////////////////////////////
class NetServer : public basic::Logger, public util::MsgObject {
public:
    NetServer(void);
    virtual ~NetServer(void);

    int start(const std::string &ip, int port, ptl::ProtocolType type);
    int stop(void);

    int close_client(client_id_t cid);
    ssize_t send_data(client_id_t cid, const ByteBuffer &buff);

    virtual int handle_msg(client_id_t cid, ByteBuffer &buffer);
    virtual int handle_msg(client_id_t cid, ptl::HttpPtl &ptl);
    virtual int handle_msg(client_id_t cid, ptl::WebsocketPtl &ptl);
    virtual int handle_client_conn(client_id_t cid);

    // 消息收到时回调函数
    virtual int msg_handler(util::obj_id_t sender, const basic::ByteBuffer &msg);
private:
    static void* client_func(void* arg); // 处理客户端发过来的数据
    static void client_conn_func(client_id_t id, void* arg); // 客户端连接时的处理函数

private:
    server_id_t id_;
    EventHandle_t handle_;

    ptl::ProtocolType type_;
    os::SocketTCP server_;
};
}

#endif