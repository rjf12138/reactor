#ifndef __URL_PARSER_H__
#define __URL_PARSER_H__

#include "basic_head.h"
#include "protocol/protocol.h"

namespace reactor {
// 格式例子: http://192.168.1.2:80/dir/index.html?uid=1
class URLParser {
public:
    URLParser(void);
    URLParser(const std::string &url);
    ~URLParser(void);

    // 清除之前保存内容
    void clear(void);
    // 解析是否成功
    bool state(void) const {return state_;}
    // 解析url
    int parser(const std::string &url);
    
public:
    bool state_;
    ptl::ProtocolType type_;    // 协议类型
    std::string addr_;  // 服务器地址
    int port_;      // 服务器端口
    std::string res_path_; // 资源路径
    std::map<std::string, std::string> param_; // 参数
};
}

#endif