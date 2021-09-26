#ifndef __HTTP_H_
#define __HTTP_H_

#include "http_def.h"
#include "basic/basic_head.h"
#include "basic/byte_buffer.h"

namespace ptl {

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

}
#endif