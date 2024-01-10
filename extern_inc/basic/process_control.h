#ifndef __PROCESS_CONTROL_H__
#define __PROCESS_CONTROL_H__

// 通用的宏工具
// 检查返回值，如果符合要求则退出函数、退出循环、继续循环、退出switch
#define RETURN_FUNC_NE(x, y, ret) if ((x) != (y)) { return ret;} else {/*什么都不做*/} // 如果满足x!=y，则退出函数，返回值 ret，否则什么都不做
#define RETURN_FUNC_EQ(x, y, ret) if ((x) == (y)) { return ret;} else {/*什么都不做*/} // 如果满足x==y，则退出函数，返回值 ret，否则什么都不做
#define RETURN_FUNC_GT(x, y, ret) if ((x) > (y)) { return ret;} else {/*什么都不做*/} // 如果满足x > y，则退出函数，返回值 ret，否则什么都不做
#define RETURN_FUNC_LT(x, y, ret) if ((x) < (y)) { return ret;} else {/*什么都不做*/} // 如果满足x < y，则退出函数，返回值 ret，否则什么都不做
#define RETURN_FUNC_GE(x, y, ret) if ((x) >= (y)) { return ret;} else {/*什么都不做*/}  // 如果满足x >= y，则退出函数，返回值 ret，否则什么都不做
#define RETURN_FUNC_LE(x, y, ret) if ((x) <= (y)) { return ret;} else {/*什么都不做*/}  // 如果满足x <= y，则退出函数，返回值 ret，否则什么都不做

#define BREAK_FUNC_EQ(x, y) if ((x) == (y)) { break; } else {/*什么都不做*/} // 如果满足x == y，则退出循环，否则什么都不做
#define BREAK_FUNC_NE(x, y) if ((x) != (y)) { break; } else {/*什么都不做*/} // 如果满足x != y，则退出循环，否则什么都不做
#define BREAK_FUNC_GT(x, y) if ((x) > (y)) { break; } else {/*什么都不做*/} // 如果满足x > y，则退出循环，否则什么都不做
#define BREAK_FUNC_LT(x, y) if ((x) < (y)) { break; } else {/*什么都不做*/} // 如果满足x < y，则退出循环，否则什么都不做
#define BREAK_FUNC_GE(x, y) if ((x) >= (y)) { break; } else {/*什么都不做*/} // 如果满足x >= y，则退出循环，否则什么都不做
#define BREAK_FUNC_LE(x, y) if ((x) <= (y)) { break; } else {/*什么都不做*/} // 如果满足x <= y，则退出循环，否则什么都不做

#define CONTINUE_FUNC_EQ(x, y) if ((x) == (y)) { continue; } else {/*什么都不做*/}  // 如果满足x == y，则继续循环，否则什么都不做
#define CONTINUE_FUNC_NE(x, y) if ((x) != (y)) { continue; } else {/*什么都不做*/}  // 如果满足x != y，则继续循环，否则什么都不做
#define CONTINUE_FUNC_GT(x, y) if ((x) > (y)) { continue; } else {/*什么都不做*/}  // 如果满足x > y，则继续循环，否则什么都不做
#define CONTINUE_FUNC_LT(x, y) if ((x) < (y)) { continue; } else {/*什么都不做*/}  // 如果满足x < y，则继续循环，否则什么都不做
#define CONTINUE_FUNC_GE(x, y) if ((x) >= (y)) { continue; } else {/*什么都不做*/}  // 如果满足x >= y，则继续循环，否则什么都不做
#define CONTINUE_FUNC_LE(x, y) if ((x) <= (y)) { continue; } else {/*什么都不做*/}  // 如果满足x <= y，则继续循环，否则什么都不做

#endif