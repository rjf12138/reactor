#ifndef __SERVER_H__
#define __SERVER_H__

#include "reactor.h"
#include "protocol/protocol.h"

namespace reactor {
// TODO: 
// 1. 收到数据时，client_func如何知道是哪个客户端socket
// 2. 如何关闭单个客户端socket
// 3. 停止服务器
class Server : public Logger {
public:
    Server(void);
    virtual ~Server(void);

    int start(const std::string &ip, int port, ptl::ProtocolType type);
    int stop(void);

    virtual int handle_msg(ByteBuffer &buffer);
    virtual int handle_msg(ptl::HttpPtl &ptl);
    virtual int handle_msg(ptl::WebsocketPtl &ptl);

private:
    static void* client_func(void* arg); // 处理客户端发过来的数据

private:
    EventHandle_t handle_;

    ptl::ProtocolType type_;
    os::SocketTCP server_;
};

}

#endif