#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "reactor.h"
#include "protocol/protocol.h"

namespace reactor {

class Client {
public:
    Client(void);
    virtual ~Client(void);

    // addr 格式：tcp: TCP://IP:Port/Ptl=[ws/http/raw], UDP: UDP://IP:Port/Ptl=[ws/http/raw], Proc: PROC://NAME:ID/Ptl=[ws/http/raw]
    // TCP/UDP ===> EventMethod_Epoll
    // PROC=====>OTHER(目前不支持)
    int connect(const std::string &addr, bool auto_reconnect = false);
    int disconnect(void);

    virtual int handle_msg(ByteBuffer &buffer, ByteBuffer &send_buf, bool &is_send);
    virtual int handle_msg(ptl::HttpPtl &ptl, ByteBuffer &send_buf, bool &is_send);
    virtual int handle_msg(ptl::WebsocketPtl &ptl, ByteBuffer &send_buf, bool &is_send);

private:
    static void* client_func(void* arg);// arg: EventHandle_t

private:
    bool auto_reconnect_;
    bool is_connected_;

    int port;
    std::string ip_;

    ptl::ProtocolType type_;
};

}

#endif