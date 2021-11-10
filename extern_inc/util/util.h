#ifndef __UTIL_H__
#define __UTIL_H__

#include "basic/byte_buffer.h"
#include "basic/wejson.h"
#include "system/system.h"
#include "data_structure/heap.h"

namespace util
{
////////////////////////// Timer //////////////////////////////////////////
// 说明： 定时器最小能设置等待间隔时间是 5 ms
enum TimerEventAttr {
    TimerEventAttr_Exit, // 定时器到期后删除当前事件
    TimerEventAttr_ReAdd, // 定时器到期后重新添加
};

typedef void* (*TimeEvent_callback_t)(void*);
typedef struct TimerEvent {
    int id; // 添加到定时器中时会返回一个ID
    uint32_t expire_time; // 定时器触发的时间，不能小于当前时间
    uint32_t wait_time; // 定时器等待的间隔时间。单位: ms, 必须大于 0
    void* TimeEvent_arg; // 回调函数的参数
    TimeEvent_callback_t TimeEvent_callback; // 定时器到期时的回调函数
    TimerEventAttr attr;

    bool operator>(const TimerEvent &rhs) {
        return this->expire_time > rhs.expire_time;
    }

    bool operator<(const TimerEvent &rhs) {
        return this->expire_time < rhs.expire_time;
    }

    bool operator==(const TimerEvent &rhs) {
        return this->expire_time == rhs.expire_time;
    }

    TimerEvent(void)
    :id(0),
    expire_time(0),
    wait_time(0),
    attr(TimerEventAttr_Exit),
    TimeEvent_arg(nullptr),
    TimeEvent_callback(nullptr) {}

    ~TimerEvent(void) {}
} TimerEvent_t;

class Timer : public os::Thread {
public:
    Timer(void);
    ~Timer(void);

    // 添加定时器,错误返回-1， 成功返回一个定时器id
    int add(TimerEvent_t &event);
    // 根据定时器ID，取消定时器
    int cancel(int id);

private:
    virtual int run_handler(void);
    virtual int stop_handler(void);

private:
    bool exit_;
    os::Time time_;
    uint32_t loop_gap_;  // 单位： ms
    uint32_t timer_id_;

    os::Mutex mutex_;
    ds::MinHeap<TimerEvent_t> timer_heap_;
};

/////////////////////////// 消息对象 ////////////////////////////////////////////////
// TODO: 当前不支持二进制数据传输
#define INVALID_ID  0

#define RECEIVER_OBJECT_ID  "recv_id"
#define SENDER_OBJECT_ID    "sender_id"
#define MSG_CONTENT         "msg_content"

#define MAX_MSG_SIZE        250 * 1024 * 1024 // 消息最大不能超过250M
#define MAX_HANDER_THREAD   4

enum MsgType {
    MsgType_Text = 1,
    MsgType_Binary = 2
};

typedef struct MsgBuffer_Info {
    basic::ByteBuffer buffer;
    os::Mutex mutex;
} MsgBuffer_Info_t;

typedef uint32_t obj_id_t;

class MsgObject {
    typedef std::map<int, MsgObject*>  MSG_OBJECT_MAP;
    typedef std::map<std::string, std::pair<obj_id_t, std::set<obj_id_t>>>  SUBSCRIBE_TOPIC_OBJECTS_MAP;
public:
    MsgObject(void);
    virtual ~MsgObject(void);

    // 获取对象 ID
    obj_id_t id(void) const {return id_;}

    // 消息收到时回调函数
    virtual int msg_handler(obj_id_t sender, const basic::ByteBuffer &msg);
    // 发送消息（使用当前消息类作为发送ID）
    int send_msg(obj_id_t recv_id, const basic::ByteBuffer &msg);

    // 检查对象 ID 是否存在
    static bool check_id(const obj_id_t &id);
    // 发送消息
    static int send_msg(obj_id_t recv_id, const basic::ByteBuffer &msg, obj_id_t sender_id);
private:
    obj_id_t id_; // 对象ID

public: 
// 下面是观察者模式的相关模式
// 创建和删除主题
// 消息对象可以订阅对应主题
// 主题的创建者通过主题发布消息，订阅该主题的对象都能收到该消息

    // 创建主题
    int create_topic(const std::string &topic);
    // 删除主题
    int delete_topic(const std::string &topic);
    // 发布消息
    int publish_msg(const std::string &topic, const basic::ByteBuffer &msg);

    // 订阅主题
    int subscribe_to_topic(const std::string &topic);
    // 取消订阅
    int unsubscribe_topic(const std::string &topic);

    // 获取主题的发布者
    obj_id_t get_topic_publisher(const std::string &topic);

private:
    std::set<std::string> topic_;// 当前对象创建的主题
    static os::Mutex topic_lock_;
    static SUBSCRIBE_TOPIC_OBJECTS_MAP subscribe_object_; // 所有主题以及订阅主题的对象

// 消息转发以及对象ID维护的相关函数
// 处理消息转发启动和停止，消息对象注册和注销
// 当得知接受者的对象id时，可以通过对象id发送消息
private:
    // 开启消息处理
    static int start(void);
    // 停止消息处理
    static void* stop(void* arg) {is_running = false; return nullptr;}
    // 生成新的对象id
    static obj_id_t next_id(void);

    // 注册对象类
    static int register_object(MsgObject *obj_ptr);
    // 移除对象类
    static int remove_object(const obj_id_t &id);

    // 消息转发中心
    static void* message_forwarding_center(void *arg);

private:
    static os::Mutex run_lock_; 
    static bool is_running;
    static obj_id_t next_object_id_;

    // 注册的消息对象
    static os::Mutex obj_lock_;
    static MSG_OBJECT_MAP objects_;

    // 消息缓冲区
    static os::ThreadPool msg_handle_pool_;
    static MsgBuffer_Info_t msg_buffer_[MAX_HANDER_THREAD];
};
} // namespace util


#endif