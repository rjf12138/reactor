#include "epoll.h"

namespace reactor {
EventManager::EventManager(EventMethod method) {}
EventManager::~EventManager(void) {}

int 
EventManager::event_init(int size)
{
    switch (method_)
    {
    case EventMethod_Epoll: {
        return dynamic_cast<Epoll*>(event_)->event_init(size);
    } break;
    
    default:
        break;
    }

    return -1;
}

int 
EventManager::event_ctl(event_handle_t handle, EventOperation op, events &event)
{

}

int 
EventManager::event_wait(events_t *events, int max_size, int timeout)
{

}
}