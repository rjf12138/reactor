#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "reactor.h"
#include "protocol/protocol.h"

namespace reactor {
// Client 不需要使用reactor,直接使用socket连接到服务器上就行了，只需维护这一条链接
class Client : public Logger{
public:
    Client(void);
    virtual ~Client(void);
    
    int connect(const std::string &url);
    int reconnect(void);
    int disconnect(void);

    virtual int handle_msg(ByteBuffer &buffer);
    virtual int handle_msg(ptl::HttpPtl &ptl);
    virtual int handle_msg(ptl::WebsocketPtl &ptl);

private:
    int connect_v();
    static void* client_func(void* arg);// arg: EventHandle_t

private:
    os::SocketTCP socket_;
    URLParser url_parser_;
};

}

#endif