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

    virtual int handle_msg(ByteBuffer &buffer);
    virtual int handle_msg(ptl::HttpPtl &ptl);
    virtual int handle_msg(ptl::WebsocketPtl &ptl);

private:
    static void* client_func(void* arg); // 处理客户端发过来的数据
    static void* accept_func(void* arg); // 处理客户端连接过来的数据

private:
    ptl::ProtocolType type_;
    util::SocketTCP server_;
};

}

#endif