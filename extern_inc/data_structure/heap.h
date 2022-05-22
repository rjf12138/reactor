#ifndef __HEAP_H__
#define __HEAP_H__
#include <vector>
#include <stdexcept>

namespace ds {
template <class T>
class MinHeap {
public:
    MinHeap(void);
    ~MinHeap(void);

    // 往最小堆添加元素
    ssize_t push(const T &data);
    // 从最小堆删除元素
    ssize_t pop(T &data);
    // 强制从堆中删除一个元素
    ssize_t remove(ssize_t pos);
    // 堆中元素个数
    ssize_t size(void) const {return size_;}
    bool empty(void) const {return this->size() == 0 ? true : false;}

    T& operator[](ssize_t pos);
private:
    void swap(T &p, T &q) {
        auto tmp = p;
        p = q;
        q = tmp;
    }
private:
    std::vector<T> heap_;
    ssize_t size_;
};

template <class T>
MinHeap<T>::MinHeap(void)
: heap_(1),
size_(0)
{
}

template <class T>
MinHeap<T>::~MinHeap(void)
{

}

template <class T>
ssize_t MinHeap<T>::push(const T &data)
{
    if (size_ == static_cast<ssize_t>(heap_.size()) - 1) {
        heap_.push_back(data);
    } else {
        heap_[size_ + 1] = data;
    }

    std::size_t min_pos = ++size_;
    while (true) {
        std::size_t parent_pos = min_pos / 2;
        if (parent_pos == 0) {
            break;
        }
        if (heap_[min_pos] < heap_[parent_pos]) {
            swap(heap_[min_pos], heap_[parent_pos]);
            min_pos = parent_pos;
        } else {
            break;
        }
    }
    return 1;
}

template <class T>
ssize_t MinHeap<T>::pop(T &data)
{
    if (this->empty()) {
        return 0;
    }

    data = heap_[1];
    heap_[1] = heap_[size_];
    --size_;

    ssize_t pos = 1;
    while (true)
    {
        ssize_t min_pos = pos;
        ssize_t left = min_pos * 2;
        ssize_t right = min_pos *2 + 1;
        if (left <= size_ && heap_[min_pos] > heap_[left]) {
            min_pos = left;
        }

        if (right <= size_ && heap_[min_pos] > heap_[right]) {
            min_pos = right;
        }

        if (min_pos != pos) {
            swap(heap_[pos], heap_[min_pos]);
            pos = min_pos;
        } else {
            break;
        }
    }
    return 1;
}

template <class T>
ssize_t MinHeap<T>::remove(ssize_t pos)
{
    ++pos; // heap 中坐标是从1开始，外部坐标从0开始
    if (pos < 1 || pos > size_) {
        return 0;
    }

    std::size_t min_pos = pos;
    while (true) {
        std::size_t parent_pos = min_pos / 2;
        if (parent_pos == 0) {
            break;
        }
        swap(heap_[min_pos], heap_[parent_pos]);
        min_pos = parent_pos;
    }

    T t;
    this->pop(t);
    return 1;
}

template <class T>
T& MinHeap<T>::operator[](ssize_t pos)
{
    if (pos < 0 || pos >= size_) {
        throw std::runtime_error("out of range");
    }

    return heap_[pos + 1];
}

}
#endif