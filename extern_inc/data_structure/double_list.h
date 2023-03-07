#ifndef __DOUBLE_LIST_H__
#define __DOUBLE_LIST_H__

#include <cstdint>
#include <stdexcept>

namespace ds {

template<typename V>
struct DoublyListNode {
	V value;
	DoublyListNode<V>* prev;
	DoublyListNode<V>* next;

	DoublyListNode(DoublyListNode<V>* p = nullptr, DoublyListNode<V>* n = nullptr)
		:prev(p), next(n)
	{}
	DoublyListNode(const V& v, DoublyListNode<V>* p = nullptr, DoublyListNode<V>* n = nullptr)
		:value(v), prev(p), next(n)
	{}
};

template <typename V>
class DoublyList {
public:
	DoublyList(void);
	virtual ~DoublyList(void);

	class Iterator {
		friend class DoublyList;
	public:
		Iterator(void);
		Iterator(const Iterator &iter);
		~Iterator(void);

		V& operator*();
		V* operator->();
		bool operator==(const Iterator& rhs);
		bool operator!=(const Iterator& rhs);
		Iterator& operator=(const Iterator& rhs);

		Iterator& operator++();
		Iterator operator++(int32_t); // 后置

		Iterator& operator--();
		Iterator operator--(int32_t); // 前置

	private:
		Iterator(DoublyList<V>* list);
	private:
		const DoublyList<V>* list_;
		DoublyListNode<V>* node_;
	};

	Iterator begin(void);
	Iterator end(void);

	int32_t push_back(const V& value);
	int32_t insert_front(const Iterator& iter, const V& value);
	int32_t insert_back(const Iterator& iter, const V& value);
	int32_t erase(const Iterator& iter);
	int32_t clear(void);
	uint32_t size(void) const;

private:
	DoublyListNode<V>* get_node(const Iterator& iter);

private:
	uint32_t size_;
	DoublyListNode<V> head_;
	DoublyListNode<V> tail_;
};

template <typename V>
DoublyList<V>::DoublyList(void)
	:size_(0),
	head_(nullptr, &tail_),
	tail_(&head_, nullptr)
{
}

template <typename V>
DoublyList<V>::~DoublyList(void)
{
	this->clear();
}

template <typename V>
uint32_t DoublyList<V>::size(void) const
{
	return size_;
}

template <typename V>
int32_t DoublyList<V>::clear(void)
{
	DoublyList<V>::Iterator iter = this->begin();
	while (iter != this->end()) {
		this->erase(iter);
		iter = this->begin();
	}

	return 0;
}

template <typename V>
typename DoublyList<V>::Iterator DoublyList<V>::begin(void)
{
	Iterator iter(this);
	iter.node_ = head_.next;

	return iter;
}

template <typename V>
typename DoublyList<V>::Iterator DoublyList<V>::end(void)
{
	Iterator iter(this);
	iter.node_ = &tail_;

	return iter;
}

template <typename V>
DoublyListNode<V>* DoublyList<V>::get_node(const Iterator& iter)
{
	if (iter.list_ != this) {
		return nullptr;
	}

	return iter.node_;
}

template <typename V>
int32_t DoublyList<V>::push_back(const V& value)
{
	DoublyListNode<V>* new_node = new DoublyListNode<V>(value, tail_.prev, &tail_);
	tail_.prev->next = new_node;
	tail_.prev = new_node;
	++size_;

	return 0;
}

template <typename V>
int32_t DoublyList<V>::insert_front(const Iterator& iter, const V& value)
{
	DoublyListNode<V>* curr_node = this->get_node(iter);
	if (curr_node == nullptr || curr_node == &head_) {
		throw std::runtime_error("DoublyList<V>::insert_front(): Insert value in the wrong postion.");
	}

	DoublyListNode<V>* new_node = new DoublyListNode<V>(value, curr_node->prev, curr_node);
	curr_node->prev->next = new_node;
	curr_node->prev = new_node;
	++size_;

	return 0;
}

template <typename V>
int32_t DoublyList<V>::insert_back(const Iterator& iter, const V& value)
{
	DoublyListNode<V>* curr_node = this->get_node(iter);
	if (curr_node == nullptr || curr_node == &tail_) {
		throw std::runtime_error("DoublyList<V>::insert_back(): Insert value in the wrong postion.");
	}

	DoublyListNode<V>* new_node = new DoublyListNode<V>(value, curr_node, curr_node->next);
	curr_node->next->prev = new_node;
	curr_node->next = new_node;
	++size_;

	return 0;
}

template <typename V>
int32_t DoublyList<V>::erase(const Iterator& iter)
{
	DoublyListNode<V>* curr_node = this->get_node(iter);
	if (curr_node == nullptr || curr_node == &head_ || curr_node == &tail_) {
		throw std::runtime_error("DoublyList<V>::erase(): Erase the value in the wrong postion.");
	}

	curr_node->prev->next = curr_node->next;
	curr_node->next->prev = curr_node->prev;

	--size_;
	delete curr_node;

	return 0;
}

////////////////////////////////////////////////////
template <typename V>
DoublyList<V>::Iterator::Iterator(void)
	:list_(nullptr), node_(nullptr)
{

}

template <typename V>
DoublyList<V>::Iterator::Iterator(const Iterator &iter)
{
	*this = iter;
}

template <typename V>
DoublyList<V>::Iterator::Iterator(DoublyList<V>* list)
:list_(list)
{

}

template <typename V>
DoublyList<V>::Iterator::~Iterator(void)
{

}

template <typename V>
V& DoublyList<V>::Iterator::operator*()
{
	if (list_ == nullptr || node_ == nullptr) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator*(): Iterator not pointer any DoublyList node.");
	}

	if (node_ == &list_->head_ || node_ == &list_->tail_) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator*(): Iterator not pointer any DoublyList node.");
	}

	return node_->value;
}

template <typename V>
V* DoublyList<V>::Iterator::operator->()
{
	if (list_ == nullptr || node_ == nullptr) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator*(): Iterator not pointer any DoublyList node.");
	}

	if (node_ == &list_->head_ || node_ == &list_->tail_) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator*(): Iterator not pointer any DoublyList node.");
	}

	return &(node_->value);
}

template <typename V>
bool DoublyList<V>::Iterator::operator==(const DoublyList<V>::Iterator& rhs)
{
	if (list_ == nullptr || node_ == nullptr) {
		return false;
	}

	if (list_ == rhs.list_ && node_ == rhs.node_) {
		return true;
	}

	return false;
}

template <typename V>
bool DoublyList<V>::Iterator::operator!=(const DoublyList<V>::Iterator& rhs)
{
	return !(*this == rhs);
}

template <typename V>
typename DoublyList<V>::Iterator& DoublyList<V>::Iterator::operator=(const DoublyList<V>::Iterator& rhs)
{
	list_ = rhs.list_;
	node_ = rhs.node_;

	return *this;
}

template <typename V>
typename DoublyList<V>::Iterator& DoublyList<V>::Iterator::operator++()
{
	if (list_ == nullptr || node_ == nullptr) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator++(): Iterator not pointer any DoublyList node.");
	}

	if (node_ == &list_->head_ || node_ == &list_->tail_) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator++(): Iterator not pointer any DoublyList node.");
	}

	node_ = node_->next;
	return *this;
}

template <typename V>
typename DoublyList<V>::Iterator DoublyList<V>::Iterator::operator++(int32_t)
{
	if (list_ == nullptr || node_ == nullptr) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator++(int32_t): Iterator not pointer any DoublyList node.");
	}

	if (node_ == &list_->head_ || node_ == &list_->tail_) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator++(int32_t): Iterator not pointer any DoublyList node.");
	}

	Iterator tmp = *this;
	node_ = node_->next;

	return tmp;
}

template <typename V>
typename DoublyList<V>::Iterator& DoublyList<V>::Iterator::operator--()
{
	if (list_ == nullptr || node_ == nullptr) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator--(): Iterator not pointer any DoublyList node.");
	}

	if (node_ == &list_->head_ || node_ == &list_->tail_) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator--(): Iterator not pointer any DoublyList node.");
	}

	node_ = node_->prev;
	return *this;
}

template <typename V>
typename DoublyList<V>::Iterator DoublyList<V>::Iterator::operator--(int32_t)
{
	if (list_ == nullptr || node_ == nullptr) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator--(int32_t): Iterator not pointer any DoublyList node.");
	}

	if (node_ == &list_->head_ || node_ == &list_->tail_) {
		throw std::runtime_error("DoublyList<V>::Iterator::operator--(int32_t): Iterator not pointer any DoublyList node.");
	}

	Iterator tmp = *this;
	node_ = node_->prev;

	return tmp;
}
}
#endif