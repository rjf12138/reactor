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

    // CRC 校验
    // https://github.com/whik/crc-lib-c
    uint8_t crc4_itu(uint8_t *data, uint16_t length);
    uint8_t crc5_epc(uint8_t *data, uint16_t length);
    uint8_t crc5_itu(uint8_t *data, uint16_t length);
    uint8_t crc5_usb(uint8_t *data, uint16_t length);
    uint8_t crc6_itu(uint8_t *data, uint16_t length);
    uint8_t crc7_mmc(uint8_t *data, uint16_t length);
    uint8_t crc8(uint8_t *data, uint16_t length);
    uint8_t crc8_itu(uint8_t *data, uint16_t length);
    uint8_t crc8_rohc(uint8_t *data, uint16_t length);
    uint8_t crc8_maxim(uint8_t *data, uint16_t length);//DS18B20
    uint16_t crc16_ibm(uint8_t *data, uint16_t length);
    uint16_t crc16_maxim(uint8_t *data, uint16_t length);
    uint16_t crc16_usb(uint8_t *data, uint16_t length);
    uint16_t crc16_modbus(uint8_t *data, uint16_t length);
    uint16_t crc16_ccitt(uint8_t *data, uint16_t length);
    uint16_t crc16_ccitt_false(uint8_t *data, uint16_t length);
    uint16_t crc16_x25(uint8_t *data, uint16_t length);
    uint16_t crc16_xmodem(uint8_t *data, uint16_t length);
    uint16_t crc16_dnp(uint8_t *data, uint16_t length);
    uint32_t crc32(uint8_t *data, uint16_t length);
    uint32_t crc32_mpeg_2(uint8_t *data, uint16_t length);

} // namespace algorithm


#endif