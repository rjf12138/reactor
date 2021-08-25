#ifndef __PROTOCOL__
#define __PROTOCOL__

#include "http.h"
#include "websocket.h"

namespace ptl {
    
enum ProtocolType {
    ProtocolType_Raw = 0,           // 原始数据，不做任何解析
    ProtocolType_Http = 1,          // HTTP 协议
    ProtocolType_Websocket = 2      // websocket 协议
};

};

#endif