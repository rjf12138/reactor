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

    // hash 散列 x86_32
    void murmurhash3_x86_32(const void *key, int len, uint32_t seed, void *out );
    void murmurhash3_x86_128(const void *key, int len, uint32_t seed, void *out );
    void murmurhash3_x64_128(const void *key, int len, uint32_t seed, void *out );
} // namespace algorithm


#endif