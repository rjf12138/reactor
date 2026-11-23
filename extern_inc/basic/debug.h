#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "basic_head.h"
#include <execinfo.h>

namespace basic {
extern std::string dump_stack(void);
}

#endif