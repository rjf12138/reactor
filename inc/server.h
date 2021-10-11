#ifndef __SERVER_H__
#define __SERVER_H__

#include "reactor.h"
#include "protocol/protocol.h"

namespace reactor {
class Server : public Logger {
public:
    Server(void);
    virtual ~Server(void);

    int start(const std::string &ip, int port, ptl::ProtocolType type);
    int stop(void);

    int close_client(client_id_t cid);
    ssize_t send_data(client_id_t cid, const ByteBuffer &buff);

    virtual int handle_msg(client_id_t cid, ByteBuffer &buffer);
    virtual int handle_msg(client_id_t cid, ptl::HttpPtl &ptl);
    virtual int handle_msg(client_id_t cid, ptl::WebsocketPtl &ptl);
    virtual int handle_client_conn(client_id_t cid);
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