#include "epoll.h"

namespace reactor {
Reactor::Reactor(void)
: reactor_state_(false)
{
    config_.min_work_threads_num = 6; // 2个reactor线程， 4个工作线程
    config_.max_work_threads_num = 18;// 2个reactor线程， 16个工作线程
    this->set_config(config_);
}
Reactor::~Reactor(void) {}

int 
Reactor::set_config(ReactorConfig_t config)
{
    config_ = config;
    util::ThreadPoolConfig thread_config = thread_pool_.get_threadpool_config();
    thread_config.min_thread_num = config_.min_work_threads_num;
    thread_config.max_thread_num = config_.max_work_threads_num;
    thread_config.max_waiting_task = 1000;
    thread_pool_.set_threadpool_config(thread_config);

    return 0;
}

int 
Reactor::event_init(void)
{
    thread_pool_.init();
}

int 
Reactor::event_ctl(EventHandle_t &handle)
{

}

void* 
Reactor::recv_buffer_func(void* arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    EventHandle_t *handle_ptr = nullptr;
    Reactor *reactor_ptr = (Reactor*)arg;
    while (reactor_ptr->reactor_state_ == false) {
        util::os_sleep(50); //每50ms处理一次任务

        if (reactor_ptr->block_event_.size() > 0) {
            auto iter = reactor_ptr->block_event_.begin();
            for (; iter != reactor_ptr->block_event_.end();) {
                if (iter->second->is_handling == false) {
                    auto find_iter = reactor_ptr->events_map_.find(iter->second->tcp_conn.get_socket());
                    if (find_iter != reactor_ptr->events_map_.end()) {
                        util::Task task; 
                        if (iter->second->is_accept == true && iter->second->accept_func != nullptr) {
                            task.work_func = iter->second->accept_func;
                            task.thread_arg = iter->second->accept_arg;
                            reactor_ptr->thread_pool_.add_task(task);
                        } else if (iter->second->client_func != nullptr) {
                            task.work_func = iter->second->client_func;
                            task.thread_arg = iter->second->client_arg;
                            reactor_ptr->thread_pool_.add_task(task);
                        }
                    }
                    reactor_ptr->block_event_.erase(iter++);
                } else {
                    ++iter;
                }
            }
        }

        if (reactor_ptr->recv_.size() > 0) {
            reactor_ptr->mutex_.lock();
            reactor_ptr->recv_.pop(handle_ptr);
            auto find_iter = reactor_ptr->block_event_.find(handle_ptr->tcp_conn.get_socket());
            if (find_iter != reactor_ptr->block_event_.end()) { // 在block_event中有了，不在添加了
                reactor_ptr->mutex_.unlock();
                continue;
            } else {
                if (handle_ptr->is_handling == true) { // 这个任务有线程在处理了，先放到block_event中等待处理
                    reactor_ptr->block_event_[handle_ptr->tcp_conn.get_socket()] = handle_ptr;
                    reactor_ptr->mutex_.unlock();
                    continue;
                }

                util::Task task; 
                if (handle_ptr->is_accept == true && handle_ptr->accept_func != nullptr) {
                    task.work_func = handle_ptr->accept_func;
                    task.thread_arg = handle_ptr->accept_arg;
                    reactor_ptr->thread_pool_.add_task(task);
                } else if (handle_ptr->client_func != nullptr) {
                    task.work_func = handle_ptr->client_func;
                    task.thread_arg = handle_ptr->client_arg;
                    reactor_ptr->thread_pool_.add_task(task);
                }
                handle_ptr->is_handling = true; //这个任务正在处理了
            }
            reactor_ptr->mutex_.unlock();
        }
    }
}

void* 
Reactor::send_buffer_func(void* arg)
{
    // 这个函数不需要，要发送消息时只要在 eventhandle_t 中添加一个标志位，设为true
    // 在epoll 中send会在线程中循环检查，当发现有数据要发送时，读取bytebuffer的数据
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    EventHandle_t *handle_ptr = nullptr;
    Reactor *reactor_ptr = (Reactor*)arg;
    while (reactor_ptr->reactor_state_ == false) {
        util::os_sleep(50); //每50ms处理一次任务


    }
}

}