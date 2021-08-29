#ifndef __TIMER_H__
#define __TIMER_H__
#include "basic/basic_head.h"

// TODO： 当一个定时器到期后，怎么去维护其他定时器时间
enum TimerEventAttr {
    TimerEventAttr_Exit, // 定时器到期后删除当前事件
    TimerEventAttr_Readd，// 定时器到期后重新天剑
};

typedef void* (*TimeEvent_callback_t)(void*);
typedef struct TimerEvent {
    int id; // 添加到定时器中时会返回一个ID
    uint32_t remain_time;
    uint32_t wait_time; // 单位: ms, 不能为 0
    void* TimeEvent_arg;
    TimeEvent_callback_t TimeEvent_callback; // 定时器到期时的回调函数
} TimerEvent_t;

class Timer {
public:
    Timer(void);
    ~Timer(void);

    // 添加定时器,错误返回-1， 成功返回一个定时器id
    int add(TimerEvent_t &event);
    // 取消一个定时器， 参数是添加定时器时的返回,以及定时时间
    int cancel(int id, int wait_time);
private:

private:
    std::vector<TimerEvent_t> timer_heap_;
};

#endif