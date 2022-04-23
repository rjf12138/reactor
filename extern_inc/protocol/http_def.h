#ifndef __HTTP_DEF_H__
#define __HTTP_DEF_H__

#define HTTP_VERSION    "HTTP/1.1"

// http 请求方法
#define HTTP_METHOD_UNKNOWN     "NONE"
#define HTTP_METHOD_GET         "GET"
#define HTTP_METHOD_POST        "POST"
#define HTTP_METHOD_PUT         "PUT"
#define HTTP_METHOD_DELETE      "DELETE"
#define HTTP_METHOD_HEAD        "HEAD"
#define HTTP_METHOD_OPTION      "OPTION"
#define HTTP_METHOD_RESPONE     HTTP_VERSION

// http 返回状态
// 状态码 1XX:
#define HTTP_STATUS_SwitchingProtocols  101     // 服务器根据客户端的请求切换协议。只能切换到更高级的协议
// 状态码 2XX： 表明请求被正常处理了
#define HTTP_STATUS_OK              200         // 正常请求
#define HTTP_STATUS_NoContent       204         // 处理成功，但是没有资源返回
#define HTTP_STATUS_PartialContent  206         // 客户端进行范围请求，服务器成功执行了，Content-Range 指定范围的实体内容
// 状态码 3XX： 表明浏览器需要执行某些特殊的处理以正确处理请求
#define HTTP_STATUS_MovedPermanently 301        // 永久性重定向。该状态码表示请求的资源已被分配了新的 URI
#define HTTP_STATUS_Found            302        // 临时性重定向。该状态码表示请求的资源已被分配了新的 URI,希望用户(本次)能使用新的 URI 访问 
#define HTTP_STATUS_SeeOther         303        // 该状态码表示由于请求对应的资源存在着另一个 URI,应使用 GET方法定向获取请求的资源。
#define HTTP_STATUS_NotModified      304        // 该状态码表示客户端发送附带条件的请求 2 时,服务器端允许请求访问资源,但未满足条件的情况
#define HTTP_STATUS_TemporaryRedirect 307       // 临时重定向。该状态码与 302 Found 有着相同的含义。
// 状态吗 4XX： 表明客户端是发生错误的原因所在。
#define HTTP_STATUS_BadRequest        400       // 该状态码表示请求报文中存在语法错误
#define HTTP_STATUS_Unauthorized      401       // 该状态码表示发送的请求需要有通过 HTTP 认证(BASIC 认证、DIGEST 认证)的认证信息。另外若之前已进行过 1 次请求,则表示用 户认证失败。
#define HTTP_STATUS_Forbidden         403       // 该状态码表明对请求资源的访问被服务器拒绝了。
#define HTTP_STATUS_NotFound          404       // 该状态码表明服务器上无法找到请求的资源。
// 状态吗 5XX： 表明服务器本身发生错误。
#define HTTP_STATUS_InternalServerError 500     // 该状态码表明服务器端在执行请求时发生了错误。
#define HTTP_STATUS_ServiceUnavailable  503     // 该状态码表明服务器暂时处于超负载或正在进行停机维护,现在无法处理请求。


#define HTTP_HEADER_ContentType     "Content-Type" // 报文主体的对象类型
#define HTTP_HEADER_KeepAlive       "Keep-Alive"

// HTTP 首部字段
// 通用首部字段
#define HTTP_HEADER_CacheControl    "Cache-Control" // 控制缓存的行为
#define HTTP_HEADER_Connection      "Connection"    // 逐跳首部、连接的管理
#define HTTP_HEADER_Date            "Date"          // 创建报文的日期时间
#define HTTP_HEADER_Pragma          "Pragma"        // 报文指令
#define HTTP_HEADER_Trailer         "Trailer"       // 报文末端的首部一览
#define HTTP_HEADER_TransferEncoding "Transfer-Encoding" // 指定报文主体的传输编码方式
#define HTTP_HEADER_Upgrade         "Upgrade"       // 升级为其他协议
#define HTTP_HEADER_Via             "Via"           // 代理服务器的相关信息
#define HTTP_HEADER_Warning         "Warning"       // 错误通知

// 请求首部字段
#define HTTP_HEADER_Accept          "Accept"                // 用户代理可处理的媒体类型
#define HTTP_HEADER_AcceptCharset   "Accept-Charset"        // 优先的字符集
#define HTTP_HEADER_AcceptEncoding  "Accept-Encoding"       // 优先的内容编码
#define HTTP_HEADER_AcceptLanaguage "Accept-Language"       // 优先的语言（自然语言）
#define HTTP_HEADER_Authorization   "Authorization"         // Web认证信息
#define HTTP_HEADER_Expect          "Expect"                // 期待服务器的特定行为
#define HTTP_HEADER_From            "From"                  // 用户的电子邮箱地址
#define HTTP_HEADER_Host            "Host"                  // 请求资源所在服务器
#define HTTP_HEADER_IfMatch         "If-Match"              // 比较实体标记(ETag)
#define HTTP_HEADER_IfModifiedSince "If-Modified-Since"     // 比较资源的更新时间
#define HTTP_HEADER_IfNoneMatch     "If-None-Match"         // 比较实体标记(与 If-Match 相反)
#define HTTP_HEADER_IfRange         "If-Range"              // 资源未更新时发送实体 Byte 的范围请求
#define HTTP_HEADER_IfUnmodifiedSince "If-Unmodified-Since" // 比较资源的更新时间(与If-Modified-Since相反)
#define HTTP_HEADER_MaxForwards     "Max-Forwards"          // 最大传输逐跳数
#define HTTP_HEADER_ProxyAuthorization "Proxy-Authorization" // 代理服务器要求客户端的认证信息
#define HTTP_HEADER_Range           "Range"                  // 实体的字节范围请求
#define HTTP_HEADER_Referer         "Referer"               // 对请求中 URI 的原始获取方
#define HTTP_HEADER_TE              "TE"                    // 传输编码的优先级
#define HTTP_HEADER_UserAgent      "User-Agent"             // HTTP 客户端程序的信息


// 响应首部字段
#define HTTP_HEADER_AcceptRanges        "Accept-Ranges"              // 是否接受字节范围请求
#define HTTP_HEADER_Age                 "Age"                        // 推算资源创建经过时间
#define HTTP_HEADER_ETag                "ETag"                       // 资源的匹配信息
#define HTTP_HEADER_Location            "Location"                   // 令客户端重定向至指定URI
#define HTTP_HEADER_ProxyAuthenticate   "Proxy-Authenticate"         // 代理服务器对客户端的认证信息
#define HTTP_HEADER_RetryAfter          "Retry-After"                // 对再次发起请求的时机要求
#define HTTP_HEADER_Server              "Server"                     // HTTP服务器的安装信息
#define HTTP_HEADER_Vary                "Vary"                       // 代理服务器缓存的管理信息
#define HTTP_HEADER_WWWAuthenticate     "WWW-Authenticate"            // 服务器对客户端的认证信息

// 实体首部字段
#define HTTP_HEADER_Allow		        "Allow"                 //资源可支持的HTTP方法
#define HTTP_HEADER_ContentEncoding     "Content-Encoding"		//实体主体适用的编码方式
#define HTTP_HEADER_ContentLanguage     "Content-Language"		//实体主体的自然语言
#define HTTP_HEADER_ContentLength	    "Content-Length"	    //实体主体的大小(单位:字节)
#define HTTP_HEADER_ContentLocation     "Content-Location"		//替代对应资源的URI
#define HTTP_HEADER_ContentMD5		    "Content-MD5"           //实体主体的报文摘要
#define HTTP_HEADER_ContentRange	    "Content-Range"	        //实体主体的位置范围
#define HTTP_HEADER_ContentType	        "Content-Type"	        //实体主体的媒体类型
#define HTTP_HEADER_Expires		        "Expires"               //实体主体过期的日期时间
#define HTTP_HEADER_LastModified	    "Last-Modified"	        //资源的最后修改日期时间

/// 扩展字段
#define HTTP_HEADER_SecWebSocketKey     "Sec-WebSocket-Key"
#define HTTP_HEADER_SecWebSocketVersion "Sec-WebSocket-Version"
#define HTTP_HEADER_SecWebSocketAccept  "Sec-WebSocket-Accept"

#endif