#ifndef __REACTOR_DEFINE_H__
#define __REACTOR_DEFINE_H__

#include "basic/logger.h"
#include "data_structure/queue.h"
#include "basic/byte_buffer.h"
#include "system/system.h"
#include "util/util.h"

namespace reactor {
using namespace basic;
enum EventMethod {
    EventMethod_Unknown,
    EventMethod_Epoll,
};

enum EventType {
    EventType_In    = 0x001,    // 数据可读
    EventType_Pri   = 0x002,    // 高优先级数据可读，比如TCP带外数据
    EventType_Out   = 0x004,    // 数据可写
    EventType_RDHup = 0x008,    // 连接被对方关闭，或者对方关闭了写操作
    EventType_Err   = 0x010,    // 错误
    EventType_Hup   = 0x020,    // 挂起。比如管道写端关闭后，读端描述符上将收到POLLHUP事件
    EventType_ET    = 0x040,    // 只触发一次，除非发生新的事件不然就不会在触发了
};

enum EventOperation {
    EventOperation_Add = 1, // 往事件上注册fd
    EventOperation_Mod = 2, // 修改事件上的fd
    EventOperation_Del = 3  // 删除事件上的fd
};

enum EventHandleState {  // 事件当前状态
    EventHandleState_Idle,      // 未触发任何事件
    EventHandleState_Ready,     // 事件已经触发但是还未处理
    EventHandleState_Handling,  // 正在处理就绪事件
};

typedef uint64_t sock_id_t;
typedef void (*client_conn_func_t)(sock_id_t,void*);
typedef void* (*handle_func_t)(void *);

typedef struct ClientConn {
    bool is_client;             // 是否是客户端连接，还是服务端接收的连接
    sock_id_t client_id;        // 连接socketid
    os::SocketTCP* socket_ptr;  // tcp连接

    // 接收缓存
    ByteBuffer recv_buffer;
    // 客户端连接时的处理函数
    void *client_arg;                       // 客户端类 NetClient
    void *server_arg;                       // 服务器类 NetServer
    void *connect_arg;                      // 连接类 ClientConn
    handle_func_t client_func;              // 处理连接发送的数据
    client_conn_func_t client_conn_func;    // 连接时候调用
    
    ClientConn(void)
    :socket_ptr(nullptr)
    , is_client(false) {
        socket_ptr = new os::SocketTCP();
    }

    ~ClientConn(void) {
        if (socket_ptr != nullptr) {
            socket_ptr->close();
            delete socket_ptr;
        }
    }
} ClientConn_t;

typedef struct EventHandle {
    os::SocketTCP *acceptor;    // 监听套接字连接

    os::Mutex *mutex;     //互斥锁，防止多线程访问上层服务/客户端资源冲突

    // 客户端连接时的处理函数
    void *server_arg;                       // 服务器类 NetServer
    void *connect_arg;
    handle_func_t client_func;
    client_conn_func_t client_conn_func;
    
    EventHandle(void)
    : acceptor(nullptr),
    connect_arg(nullptr),
    client_func(nullptr),
    client_conn_func(nullptr)
    {}
} EventHandle_t;

typedef struct ReactorConfig {
    bool is_start_server;
    uint32_t sub_reactor_size_;
    uint32_t main_reactor_max_events_size_;
    uint32_t sub_reactor_max_events_size_;

    ReactorConfig()
    :is_start_server(true),
    sub_reactor_size_(5),
    main_reactor_max_events_size_(32),
    sub_reactor_max_events_size_(32) {}
} ReactorConfig_t;
}

#endif