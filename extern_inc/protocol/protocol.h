#ifndef __PROTOCOL__
#define __PROTOCOL__

#include "http_def.h"
#include "basic/basic_head.h"
#include "basic/byte_buffer.h"

namespace ptl {
    
enum ProtocolType {
    ProtocolType_Raw = 0,           // 原始数据，不做任何解析
    ProtocolType_Http = 1,          // HTTP 协议
    ProtocolType_Websocket = 2      // websocket 协议
};

/////////////////////// URL Parser //////////////////////////////////
// 协议名称：
// raw: raw         // 原始收到的数据不做任何解析
// http: http       // http 协议。 默认端口： 80
// websocket: ws    // websocket 协议。 默认端口： 80

// 格式例子: http://192.168.1.2:80/dir/index.html?uid=1&key=2

enum ParserError {
    ParserError_Ok = 0,                 // 解析正确
    ParserError_UnknownPtl = -1,        // 协议不确定
    ParserError_IncompleteURL = -2,     // url 不完整
    ParserError_AmbiguousPort = -3,     // 端口不明确
    ParserError_ErrorPort = -4,         // 端口错误
    ParserError_IncompleteParameters = -5 // 参数不全
};

class URLParser {
public:
    URLParser(void);
    ~URLParser(void);

    // 清除之前保存内容
    void clear(void);
    // 解析url
    ParserError parser(const std::string &url);
    
public:
    ptl::ProtocolType type_;    // 协议类型
    std::string addr_;  // 服务器地址
    int port_;      // 服务器端口
    std::string res_path_; // 资源路径
    std::map<std::string, std::string> param_; // 参数
};

/////////////////////// HTTP Parser  ////////////////////////////////
enum HttpParse_ErrorCode {
    HttpParse_OK = 0,
    HttpParse_CantFindHttp = -1,
    HttpParse_CantFindBody = -2,
    HttpParse_ContentNotEnough = -3,
    HttpParse_HttpVersionNotMatch = -4,
    HttpParse_ParseHeaderFailed = -5,
    HttpParse_NotSupportHttp = -6
};

class HttpPtl {
public:
    HttpPtl(void);
    virtual ~HttpPtl(void);

    HttpParse_ErrorCode parse(basic::ByteBuffer &data);
    int generate(basic::ByteBuffer &data);

    int clear();

    // 设置http请求头
    int set_request(const std::string &method, const std::string &url);
    // 设置http回应头
    int set_response(int code, const std::string &phrase);
    // 设置回应消息短语
    int set_phrase(const std::string &phrase);
    // 设置报文主体内容
    int set_content(const basic::ByteBuffer &data);
    // 设置头选项
    int set_header_option(const std::string &key, const std::string &value);

    // 获取回应状态码
    int get_status_code(void);
    // 获取请求URL
    std::string get_url(void);
    // 获取http方法
    std::string get_method(void);
    // 获取消息短语
    std::string get_phrase(void);
    // 获取消息短语
    std::string get_header_option(const std::string &key);
    // 获取报文内容
    basic::ByteBuffer& get_content(void);

private:
    bool is_request_;
    int code_;
    std::string url_;
    std::string method_;
    std::string phrase_;
    basic::ByteBuffer content_;
    std::map<std::string, std::string> header_;
};

////////////////////////////// websocket parser //////////////////////////////////////////////////
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
    // 检查服务端请求是否正确
    int check_upgrade_response(HttpPtl &response);

    // 获取消息内容
    basic::ByteBuffer& get_content(void);
    // 获取opcode
    ENUM_WEBSOCKET_OPCODE get_opcode(void);
    // 清空
    void clear(void);

    // 打印成16进制
    int32_t print_hex(int8_t val);
private:
    int generate_sec_websocket_key(basic::ByteBuffer &out);
    int generate_sec_websocket_accept(basic::ByteBuffer &sec_key);

    // 64位整型大小端转换
    uint64_t ntohll(uint64_t val);
    uint64_t htonll(uint64_t val);

    // 检查大小端
    int32_t check_end(void);
private:
    int8_t fin_;
    basic::ByteBuffer sec_websocket_accept_;
    ENUM_WEBSOCKET_OPCODE opcode_;

    basic::ByteBuffer data_;
};
};

#endif