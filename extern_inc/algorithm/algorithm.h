#ifndef _SHA1_H_
#define _SHA1_H_

#include <stdint.h>
#include "basic/byte_buffer.h"

namespace algorithm
{
    // sha1
    extern ssize_t sha1(basic::ByteBuffer &inbuf, basic::ByteBuffer &outbuf);
    // base64 编码和解码
    extern ssize_t encode_base64(basic::ByteBuffer &inbuf, basic::ByteBuffer &outbuf);
    extern ssize_t decode_base64(basic::ByteBuffer &inbuf, basic::ByteBuffer &outbuf);

} // namespace algorithm


#endif