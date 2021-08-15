// 模板类定义声明要在同一个文件中
namespace ds {

template <class T>
struct Element {
    T data;

    Element *pre_node;
    Element *next_node;

    Element(void);
    virtual ~Element(void);
};


template<class T>
Element<T>::Element(void)
: pre_node(nullptr), next_node(nullptr)
{}

template<class T>
Element<T>::~Element(void)
{}

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
    Element<T> *head_;
    Element<T> *tail_;
    int size_;
};


template<class T>
Queue<T>::Queue(void)
: head_(nullptr), tail_(nullptr), size_(0)
{
}

template<class T>
Queue<T>::~Queue()
{
    if (size_ >= 0) {
        while (head_ != tail_) {
            Element<T> *del_node = head_;
            if (head_ == tail_) {
                head_ = tail_ = nullptr;
            } else {
                head_ = head_->next_node;
            }
            delete del_node;
            del_node = nullptr;
        }
    }
}

template<class T>
int Queue<T>::push(const T &data)
{
    Element<T> *node = new Element<T>();
    node->data = data;

    if (head_ == nullptr && tail_ == nullptr) {
        head_ = node;
        tail_ = node;
    } else {
        tail_->next_node = node;
        tail_ = node;
    }
    ++size_;

    return 1;
}

template<class T>
int Queue<T>::pop(T &data)
{
    if(size_ == 0){
        return 0;
    }
    
    data = head_->data;

    Element<T> *del_node = head_;
    if (head_ == tail_) {
        head_ = tail_ = nullptr;
    } else {
        head_ = head_->next_node;
    }
    --size_;

    delete del_node;
    del_node = nullptr;

    return 1;
}


template<class T> 
inline int Queue<T>::size(void) const
{
    return size_;
}

template<class T>
inline bool Queue<T>::empty(void) const
{
    return size_ == 0 ? true : false;
}

}