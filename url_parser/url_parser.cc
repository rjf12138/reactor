#include "url_parser.h"

namespace reactor {
// 协议名称：
// raw: raw         // 原始收到的数据不做任何解析
// http: http       // http 协议。 默认端口： 80
// websocket: ws    // websocket 协议。 默认端口： 80
// 

enum ParserError {
    ParserError_Ok = 0, // 解析正确
    ParserError_UnknownPtl = -2， // 协议不确定
    ParserError_IncompleteURL = -3，  // url 不完整
    ParserError_AmbiguousPort = -4  // 端口不明确
};

enum ParserState {
    ParserState_Protocol,
    ParserState_Addr,
    ParserState_Port,
    ParserState_ResPath,
    ParserState_Param,
    ParserState_Complete
};

URLParser::URLParser(void)
: state_(false)
{

}

URLParser::URLParser(const std::string &url)
{

}

URLParser::~URLParser(void)
{

}

void 
URLParser::clear(void)
{
    state_ = false;
    type_ = ptl::ProtocolType_Raw;
    addr_.clear();
    port_ = 0;
    res_path_.clear();
    param_.clear();
}

int 
URLParser::parser(const std::string &url)
{
    ParserState state = ParserState_Protocol;
    
    for (std::size_t i = 0; i < url.length();) {
        switch (state)
        {
        case ParserState_Protocol: {
            if (url[i] == 'h' || url[i] == 'H') {
                if (url.length() > 7 && (url.c_str(), "http://", 7) == 0) {
                    type_ = ptl::ProtocolType_Http;
                    i += 7;
                } else {
                    goto end;
                }
            } else if (url[i] == 'w' || url[i] == 'W') {
                if (url.length() > 5 && strncasecmp(url.c_str(), "ws://", 5) == 0) {
                    type_ = ptl::ProtocolType_Http;
                    i += 5;
                } else {
                    goto end;
                }
            } else if (url[i] == 'r' || url[i] == 'R') {
                if (url.length() > 6 && strncasecmp(url.c_str(), "raw://", 6) == 0) {
                    type_ = ptl::ProtocolType_Raw;
                    i += 6;
                } else {
                    goto end;
                }
            } else {
                goto end;
            }
        
        } break;
        case ParserState_Addr: {
            for (;i < url.length(); ++i) {
                if (url[i] == ':') {
                    state_ = ParserState_Port;
                    break;
                } else if (url[i] == '/') {
                    if (type_ == ptl::ProtocolType_Websocket || type_ == ptl::ProtocolType_Http) {
                        port_ = 80;
                    } else {

                    }
                }
            }
        } break;
        default:
            break;
        }
    }
end:
    if (state != ParserState_Complete) {
        return -1;
    }
    return 0;
}

}