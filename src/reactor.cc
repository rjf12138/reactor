#include "reactor.h"
#include "linux_reactor.h"
#include "reactor_define.h"

namespace reactor {

int reactor_start(const ReactorConfig_t &config)
{
    return ReactorManager::instance().start(config);
}

int reactor_stop(void) 
{
    return ReactorManager::instance().stop();
}

}