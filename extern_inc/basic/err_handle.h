#ifndef __ERR_HANDLE_H__
#define __ERR_HANDLE_H__

#include <list>
#include <string>
#include <utility>

typedef void (*error_handle_callback)(int err_no, void* data, std::string str_error);

namespace basic {

class ErrHandle {
public:
    ErrHandle(void);
    virtual ~ErrHandle(void);

    // 设置错误码
    void set_err_code(int err_code, void* data = nullptr, const std::string &str_error = "");

    // 获取最新的错误码
    int new_errno(void);

    // 增加错误回调，当错误发生时候进行处理
    void add_err_handle_callback(error_handle_callback callback);

private:
    // 错误码
    int err_code_;
    // 错误回调函数
    error_handle_callback callback_;
};

}
#endif