#include "epoll.h"

namespace reactor {
Reactor::Reactor(void)
: reactor_state_(false)
{
    config_.min_work_threads_num = 6; // 2个reactor线程， 4个工作线程
    config_.max_work_threads_num = 18;// 2个reactor线程， 16个工作线程
    this->set_config(config_);
}
Reactor::~Reactor(void) {
    thread_pool_.stop_handler();
    for (auto iter = events_map_.begin(); iter != events_map_.end(); ++iter) {
        delete iter->second;
    }
}

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

    util::Task task;
    task.work_func = Reactor::recv_buffer_func;
    task.thread_arg = this;
    task.exit_task = Reactor::reactor_exit;
    task.exit_arg = this;
    thread_pool_.add_task(task);
    memset(&task, 0, sizeof(task));

    // 创建epoll事件处理
    Epoll *epoll_ptr = new Epoll(&mutex_, &recv_);
    task.work_func = Epoll::event_wait;
    task.thread_arg = epoll_ptr;
    task.exit_task = Epoll::event_exit;
    task.exit_arg = epoll_ptr;
    thread_pool_.add_task(task);
    events_map_[(uint64_t)epoll_ptr] = epoll_ptr;
}

int 
Reactor::event_ctl(EventHandle_t &handle)
{
    if (handle.method == EventMethod_Epoll) {
        for (auto iter = events_map_.begin(); iter != events_map_.end(); ++iter) {
            if ((*iter->second).get_type() == EventMethod_Epoll) {
                (*iter->second).event_ctl(handle);
            }
        }
    } else {
        return -1;
    }
    return 0;
}

void* 
Reactor::reactor_exit(void* arg)
{
    if (arg == nullptr) {
        LOG_GLOBAL_ERROR("arg is nullptr");
        return nullptr;
    }

    EventHandle_t *handle_ptr = nullptr;
    Reactor *reactor_ptr = (Reactor*)arg;

    reactor_ptr->reactor_state_ = false;

    return nullptr;
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

}