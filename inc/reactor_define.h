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

typedef uint32_t client_id_t;
typedef uint64_t server_id_t;
typedef struct ClientConn {
    client_id_t client_id;
    os::SocketTCP* socket_ptr;

    os::Mutex buff_mutex;
    ByteBuffer send_buffer;
    ByteBuffer recv_buffer;
    
    ClientConn(void)
    :socket_ptr(nullptr) {
        socket_ptr = new os::SocketTCP();
    }

    ~ClientConn(void) {
        if (socket_ptr != nullptr) {
            socket_ptr->close();
            delete socket_ptr;
        }
    }
} ClientConn_t;

typedef void (*client_conn_func_t)(client_id_t,void*);
typedef void* (*handle_func_t)(void *);
typedef struct EventHandle {
    bool exit;
    server_id_t server_id;
    os::SocketTCP *acceptor;    // 监听套接字连接

    os::Mutex client_conn_mutex;
    std::map<client_id_t, ClientConn_t*> client_conn; // 客户端连接

    os::Mutex ready_sock_mutex;
    ds::Queue<int> ready_sock;      // 就绪的客户端描述符列表

    EventHandleState state;     // 事件当前状态

    uint32_t events;            // 事件集合
    EventMethod method;         // 哪中类型的event, 目前只有epoll

    // 客户端连接时的处理函数
    void *client_arg;
    handle_func_t client_func;
    client_conn_func_t client_conn_func;

    EventHandle(void)
    : acceptor(nullptr),
    client_arg(nullptr),
    client_func(nullptr),
    client_conn_func(nullptr),
    state(EventHandleState_Idle)
    {}
} EventHandle_t;

typedef struct ReactorConfig {
    bool is_start_server;
    uint32_t threads_num;
    uint32_t send_thread_num;
    uint32_t max_wait_task;

    ReactorConfig()
    :is_start_server(true),
    threads_num(5),
    send_thread_num(1),
    max_wait_task(10000) {}
} ReactorConfig_t;
}

#endif