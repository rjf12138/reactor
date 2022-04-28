#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "basic_head.h"

namespace basic {

#define MAX_BUFFER_SIZE     1073741824 // 1*1024*1024*1024 (1GB)
#define MAX_DATA_SIZE       1073741823 // 多的一个字节用于防止，缓存写满时，start_write 和 start_read 重合而造成分不清楚是写满了还是没写

typedef char bufftype;
typedef char* buffptr;

class ByteBufferIterator;
class ByteBuffer {
    friend class ByteBufferIterator;
public:
    typedef ByteBufferIterator iterator;
    typedef const ByteBufferIterator const_iterator;
public:
    ByteBuffer(ssize_t size = 0);
    ByteBuffer(const ByteBuffer &buff);
    ByteBuffer(const std::string &str);
    ByteBuffer(const buffptr data, ssize_t size);
    virtual ~ByteBuffer();

    ssize_t read_int8(int8_t &val);
    ssize_t read_int16(int16_t &val);
    ssize_t read_int32(int32_t &val);
    ssize_t read_int64(int64_t &val);
    ssize_t read_string(std::string &str, ssize_t str_size = -1);
    ssize_t read_bytes(void *buf, ssize_t buf_size);

    ssize_t write_int8(int8_t val);
    ssize_t write_int16(int16_t val);
    ssize_t write_int32(int32_t val);
    ssize_t write_int64(int64_t val);
    ssize_t write_string(const std::string &str, ssize_t str_size = -1);
    ssize_t write_bytes(const void *buf, ssize_t buf_size);

    bool empty(void) const;
    ssize_t data_size(void) const;
    ssize_t idle_size(void) const;
    ssize_t clear(void);

    // 重新分配缓冲区大小(只能向上增长), size表示重新分配缓冲区的下限
    ssize_t resize(ssize_t size);

    // 重载操作符
    ByteBuffer& operator+(const ByteBuffer &rhs);
    ByteBuffer& operator+=(const ByteBuffer &rhs);
    bool operator==(const ByteBuffer &rhs) const;
    bool operator!=(const ByteBuffer &rhs) const;
    ByteBuffer& operator=(const ByteBuffer& src);

    bufftype& operator[](ssize_t index);
    bufftype& operator[](const ssize_t &index) const;

    // 返回起始结束迭代器
    iterator begin(void);
    iterator end(void);
    iterator last_data(void);
    
    // 返回const 起始结束迭代器
    const_iterator begin(void) const;
    const_iterator end(void) const;
    const_iterator last_data(void) const;

    // 判断 patten 是不是 bytebuffer 从 iter 开始的子串, size: -1 表示匹配全部, 否则指定具体大小
    bool bytecmp(ByteBufferIterator &iter, ByteBuffer &patten, ssize_t size = -1);
    // 将ByteBuffer中数据以字符串形式返回
    std::string str();
    // 获取 ByteBuffer 迭代器指定范围的数据
    ssize_t get_data(ByteBuffer &out, ByteBufferIterator &copy_start, ssize_t copy_size);
    // 只读不修改读位置
    ssize_t read_only(ssize_t start_pos, void *buf, ssize_t buf_size);
    //////////////////////////////////////////////////

    // 向外面直接提供 buffer_ 指针，它们写是直接写入指针，避免不必要的拷贝
    buffptr get_write_buffer_ptr(void) const;
    buffptr get_read_buffer_ptr(void) const;

    // ByteBuffer 是循环队列，读写不一定是连续的
    ssize_t get_cont_write_size(void) const;
    ssize_t get_cont_read_size(void) const;

    // 更新读写数据和剩余的缓冲大小
    ssize_t update_write_pos(ssize_t offset);
    ssize_t update_read_pos(ssize_t offset);

    // ===================== 操作ByteBuffer ======================
    // 返回 ByteBuffer 中所有匹配 buff 的迭代器
    std::vector<ByteBufferIterator> find(const ByteBuffer &buff);
    
    // 根据 buff 分割 ByteBuffer
    std::vector<ByteBuffer> split(const ByteBuffer &buff);

    // 将 Bytebuffer 中 buf1 替换为 buf2
    ByteBuffer replace(ByteBuffer buf1, const ByteBuffer &buf2, ssize_t index = -1);

    // 移除 ByteBuff 中匹配 buff 的子串
    // index 指定第几个匹配的子串， index 超出范围时，删除所有匹配子串, index 从0 开始计数
    ByteBuffer remove(const ByteBuffer &buff, ssize_t index = -1);

    // 在 ByteBuff 指定迭代器前/后插入子串 buff
    ssize_t insert_front(ByteBufferIterator &insert_iter, const ByteBuffer &buff);
    ssize_t insert_back(ByteBufferIterator &insert_iter, const ByteBuffer &buff);

    // 返回符合模式 regex 的子串(使用正则表达式)
    std::vector<ByteBuffer> match(ByteBuffer &regex);

private:
    // 设置外部缓存
    ssize_t set_extern_buffer(buffptr exbuf, int buff_size);
    // 下一个读的位置
    void next_read_pos(int offset = 1);
    // 下一个写的位置
    void next_write_pos(int offset = 1);

    // 将data中的数据拷贝size个字节到当前bytebuff中
    ssize_t copy_data_to_buffer(const void *data, ssize_t size);
    // 从bytebuff中拷贝data个字节到data中
    ssize_t copy_data_from_buffer(void *data, ssize_t size);
    
private:
    buffptr buffer_;

    ssize_t start_read_pos_;
    ssize_t start_write_pos_;

    ssize_t used_data_size_;
    ssize_t free_data_size_;
    ssize_t max_buffer_size_;
};

// 迭代器
class ByteBufferIterator
{
    friend class ByteBuffer;
public:
    ByteBufferIterator(void);
    ByteBufferIterator(const ByteBufferIterator &rhs);
    ~ByteBufferIterator(void);

    bufftype operator*();
    ByteBufferIterator operator+(ssize_t inc);
    ByteBufferIterator operator-(int des);
    // 求出两个迭代器之间的距离绝对值
    ssize_t operator-(ByteBufferIterator &rhs);
    // 前置++
    ByteBufferIterator& operator++();
    // 后置++
    ByteBufferIterator operator++(int);
    // 前置--
    ByteBufferIterator& operator--();
    // 后置--
    ByteBufferIterator operator--(int);
    // +=
    ByteBufferIterator& operator+=(ssize_t inc);
    ByteBufferIterator& operator-=(ssize_t des);

    // 只支持 == ,!= , = 其他的比较都不支持
    bool operator==(const ByteBufferIterator& iter) const;
    bool operator!=(const ByteBufferIterator& iter) const;
    bool operator>(const ByteBufferIterator& iter) const;
    bool operator>=(const ByteBufferIterator& iter) const;
    bool operator<(const ByteBufferIterator& iter) const;
    bool operator<=(const ByteBufferIterator& iter) const;
    ByteBufferIterator& operator=(const ByteBufferIterator& src);

    std::string debug_info(void);
private:
    ByteBufferIterator(const ByteBuffer *buffer, const ssize_t &pos);
    bool check_iterator(void);
    bool move_postion(ssize_t distance, ssize_t &new_postion);

private:
    const ByteBuffer *buff_ = nullptr;
    ssize_t curr_pos_;
};

}

#endif