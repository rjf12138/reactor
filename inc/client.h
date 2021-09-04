#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "reactor.h"
#include "protocol/protocol.h"

namespace reactor {

class Client : public Logger{
public:
    Client(EventMethod method = EventMethod_Epoll);
    virtual ~Client(void);

    // TCP ===> EventMethod_Epoll
    // PROC=====>OTHER(目前不支持)
    int connect(const std::string &url);
    int reconnect(void);
    int disconnect(void);

    virtual int handle_msg(ByteBuffer &buffer, ByteBuffer &send_buf, bool &is_send);
    virtual int handle_msg(ptl::HttpPtl &ptl, ByteBuffer &send_buf, bool &is_send);
    virtual int handle_msg(ptl::WebsocketPtl &ptl, ByteBuffer &send_buf, bool &is_send);

private:
    static void* client_func(void* arg);// arg: EventHandle_t

private:
    EventMethod method_;

    util::SocketTCP socket_;
    URLParser url_parser_;
};

}

#endif