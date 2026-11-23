// 模板类定义声明要在同一个文件中
#ifndef __QUEUE_H__
#define __QUEUE_H__
#include <queue>
namespace ds {

template<class T>
class Queue {
public:
    Queue(void);
    virtual ~Queue();

    int push(const T &data);
    int pop(T &data);
    int size(void) const;
    bool empty(void) const;
    
private:
    std::queue<T> queue_;
};


template<class T>
Queue<T>::Queue(void)
{
}

template<class T>
Queue<T>::~Queue()
{
}

template<class T>
int Queue<T>::push(const T &data)
{
    queue_.push(data);
    return 1;
}

template<class T>
int Queue<T>::pop(T &data)
{
    if(queue_.empty() == true){
        return 0;
    }
    
    data = queue_.front();
    queue_.pop();

    return 1;
}


template<class T> 
inline int Queue<T>::size(void) const
{
    return static_cast<int>(queue_.size());
}

template<class T>
inline bool Queue<T>::empty(void) const
{
    return queue_.empty();
}

}
#endif