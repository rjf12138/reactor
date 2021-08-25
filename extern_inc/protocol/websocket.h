#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include "http.h"

namespace ptl {

enum WebsocketParse_ErrorCode {
    WebsocketParse_OK = 0,
    WebsocketParse_PacketNotEnough = -1,
    WebsocketParse_ParseFailed = -2,
    WebsocketParse_NotSupportWebsocket = -3
};

enum ENUM_WEBSOCKET_OPCODE {
    WEBSOCKET_OPCODE_UNKNOWN = -1,
    WEBSOCKET_OPCODE_CONTINUATION_FRAME = 0x01,
    WEBSOCKET_OPCODE_TEXT_FRAME = 0x02,
    WEBSOCKET_OPCODE_BINARY_FRAME = 0x03,
    WEBSOCKET_OPCODE_RESERVED_FOR_NON_CONTROL_FRAMES,
    WEBSOCKET_OPCODE_CONNECTION_CLOSE = 0x08,
    WEBSOCKET_OPCODE_PING = 0x09,
    WEBSOCKET_OPCODE_PONG = 0x0A,
    WEBSOCKET_OPCODE_RESERVED_FOR_CONTROL_FRAMES
};

class WebsocketPtl {
public:
    WebsocketPtl(void);
    virtual ~WebsocketPtl(void);

    WebsocketParse_ErrorCode parse(basic::ByteBuffer &buff);
    int32_t generate(basic::ByteBuffer &out, basic::ByteBuffer &content, int8_t nOpcode, bool bMask = false);

    // 客户端请求将链接升级为 websocket 的 http 请求包
    int get_upgrade_packet(HttpPtl &request, basic::ByteBuffer &content, std::string url = "/", std::string host = "websocket client");
    // 服务端回复客户端的请求
    int response_upgrade_packet(HttpPtl &request, HttpPtl &response, basic::ByteBuffer &content, std::string host = "websocket server");

    // 获取消息内容
    basic::ByteBuffer& get_content(void);
    // 获取opcode
    ENUM_WEBSOCKET_OPCODE get_opcode(void);

    // 64位整型大小端转换
    uint64_t ntohll(uint64_t val);
    uint64_t htonll(uint64_t val);

    // 检查大小端
    int32_t check_end(void);
    // 打印成16进制
    int32_t print_hex(int8_t val);

    // 清空
    void clear(void);
private:
    int generate_sec_websocket_key(basic::ByteBuffer &out);
    int generate_sec_websocket_accept(basic::ByteBuffer &sec_key);
private:
    int8_t fin_;
    basic::ByteBuffer sec_websocket_accept_;
    ENUM_WEBSOCKET_OPCODE opcode_;

    basic::ByteBuffer data_;
};

}
#endif