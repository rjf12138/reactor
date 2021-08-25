#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "reactor.h"
#include "protocol/protocol.h"

namespace reactor {

class Client {
public:
    Client(void);
    virtual ~Client(void);

    int connect(const std::string &ip, int port, ptl::ProtocolType type = ptl::ProtocolType_Raw, bool auto_reconnect = false);
    int disconnect(void);

    virtual int recv(ByteBuffer &buffer, ByteBuffer &send_buf, bool &is_send);
    virtual int recv(ptl::HttpPtl &ptl, ByteBuffer &send_buf, bool &is_send);
    virtual int recv(ptl::WebsocketPtl &ptl, ByteBuffer &send_buf, bool &is_send);

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