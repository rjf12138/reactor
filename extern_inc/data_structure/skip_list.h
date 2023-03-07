#ifndef __SKIP_LIST_H__
#define __SKIP_LIST_H__

#include <iostream>
#include <cstdint>
#include <stdexcept>

namespace ds {
#define MAX_LEVEL   32
template<typename K>
class Compare {
public:
	int32_t operator()(K key1, K key2) {
		if (key1 > key2) {
			return 1;
		} else if (key1 < key2) {
			return -1;
		}

		return 0;
	}
};

template<typename K, typename V>
struct SkipListNode {
	K key;
	V value;
	int8_t level;
	SkipListNode<K, V>* next[MAX_LEVEL];
};

template<typename K, typename V, typename Comparator = Compare<K>>
class SkipList {
public:
	SkipList(void);
	virtual ~SkipList(void);

	class Iterator {
		friend class SkipList;
	public:
		Iterator(void);
		Iterator(const Iterator &iter);
		~Iterator(void);

		K& key();
		V& value();
		bool operator==(const Iterator& rhs) const;
		bool operator!=(const Iterator& rhs) const;
		Iterator& operator=(const Iterator& rhs);

		Iterator& operator++();
		Iterator operator++(int32_t); // 后置

	private:
		Iterator(SkipList<K, V,Comparator>* list);

	private:
		SkipList<K, V, Comparator>* list_;
		SkipListNode<K, V>* node_;
	};

	Iterator begin(void);
	Iterator end(void);

	bool empty(void) {return size() == 0 ? true : false;}
	Iterator get(const K &key);
	Iterator put(const K &key, const V &value);
	Iterator erase(const K &key);
	Iterator erase(Iterator &iter);

	int32_t size(void);

    void print(void);
    // void print_rand(void);

private:
	int8_t random_height(void);

private:
	int32_t size_;
	Comparator compare_;
	SkipListNode<K, V> header_;
	SkipListNode<K, V> tail_;
};

template<typename K, typename V, class Comparator>
SkipList<K, V, Comparator>::SkipList(void)
	:size_(0)
{
	header_.level = MAX_LEVEL;
	for (int32_t i = 0; i <= header_.level; ++i) {
		header_.next[i] = &tail_;
	}
}

template<typename K, typename V, class Comparator>
SkipList<K, V, Comparator>::~SkipList(void)
{
	SkipListNode<K, V>* next_ptr = header_.next[0];
	while (next_ptr != &tail_) {
		SkipListNode<K, V>* remove_ptr = next_ptr;
		next_ptr = remove_ptr->next[0];
		delete remove_ptr;
	}
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V, Comparator>::Iterator SkipList<K, V, Comparator>::get(const K &key)
{
	int8_t level = header_.level;
	SkipListNode<K, V>* ptr = &header_;
	while (level >= 0) {
		if (ptr->next[level] == &tail_ || compare_(ptr->next[level]->key, key) == 1) {
			if (level > 0) {
				--level;
			}
			else {
				break;
			}
		} else if (compare_(ptr->next[level]->key, key) == -1) {
			ptr = ptr->next[level];
		} else if (compare_(ptr->next[level]->key, key) == 0) {
			Iterator iter(this);
			iter.node_ = ptr->next[level];
			return iter;
		}
	}

	return end();
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V, Comparator>::Iterator SkipList<K, V, Comparator>::put(const K &key, const V &value)
{
	int8_t level = header_.level;
	SkipListNode<K, V>* ptr = &header_;
	SkipListNode<K, V>* new_node = new SkipListNode<K, V>;
	new_node->key = key;
	new_node->value = value;
	new_node->level = this->random_height();

	while (level >= 0) {
		if (ptr->next[level] == &tail_ || compare_(ptr->next[level]->key, key) == 1) {
			if (level <= new_node->level) {
				new_node->next[level] = ptr;
			}

			if (level > 0) {
				--level;
			} else {
				break;
			}
		} else if (compare_(ptr->next[level]->key, key) == -1) {
			ptr = ptr->next[level];
		} else if (compare_(ptr->next[level]->key, key) == 0) {
			delete new_node;
			return end();
		}
	}

	for (int32_t i = new_node->level; i >= 0; --i) {
		SkipListNode<K, V>* tmp_ptr = new_node->next[i];
		if (tmp_ptr != &tail_) {
			new_node->next[i] = tmp_ptr->next[i];
			tmp_ptr->next[i] = new_node;
		} else {
			new_node->next[i] = &tail_;
		}
	}
	++size_;

	Iterator iter(this);
	iter.node_ = new_node;

	return iter;
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V, Comparator>::Iterator SkipList<K, V, Comparator>::erase(const K &key)
{
	int8_t level = header_.level;
	SkipListNode<K, V>* ptr = &header_, * remove_ptr = nullptr;
	while (level >= 0) {
		if (ptr->next[level] == &tail_ || compare_(ptr->next[level]->key, key) == 1) {
			if (level > 0) {
				--level;
			}
			else {
				break;
			}
		}
		else if (compare_(ptr->next[level]->key, key) == -1) {
			ptr = ptr->next[level];
		}
		else if (compare_(ptr->next[level]->key, key) == 0) {
			remove_ptr = ptr->next[level];
			ptr->next[level] = remove_ptr->next[level];
			--level;
		}
	}

	if (remove_ptr != nullptr) {
		--size_;
		Iterator iter(this);
		iter.node_ = remove_ptr; // 返回下一个节点
		delete remove_ptr;

		return iter;
	}


	return end();
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V, Comparator>::Iterator SkipList<K, V, Comparator>::erase(Iterator &iter)
{
	if (iter.list_ != this) {
		return end();
	}

	return erase(iter.key());
}

template<typename K, typename V, class Comparator>
int32_t SkipList<K, V, Comparator>::size(void)
{
	return size_;
}

template<typename K, typename V, class Comparator>
int8_t SkipList<K, V, Comparator>::random_height(void)
{
	int8_t height = 0;
	for (int32_t i = 0; i < MAX_LEVEL; ++i) {
		if (rand() % 2 == 1) {
			++height;
		}
	}

	return height;
}

template<typename K, typename V, class Comparator>
void SkipList<K, V,Comparator>::print(void)
{
   SkipListNode<K, V>* ptr = &header_;

   while (ptr->next[0] != &tail_) {
       std::cout << "key: " << ptr->next[0]->key << ", value: " << ptr->next[0]->value << std::endl;
       ptr = ptr->next[0];
   }
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V,Comparator>::Iterator SkipList<K, V,Comparator>::begin(void)
{
	Iterator iter(this);
	iter.node_ = header_.next();

	return iter;
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V,Comparator>::Iterator SkipList<K, V,Comparator>::end(void)
{
	Iterator iter(this);
	iter.node_ = &header_;

	return iter;
}

///////////////////////////////////////////////////////////////////////////////

template<typename K, typename V, class Comparator>
SkipList<K, V, Comparator>::Iterator::Iterator(void)
	:list_(nullptr), node_(nullptr)
{

}

template<typename K, typename V, class Comparator>
SkipList<K, V,Comparator>::Iterator::Iterator(const Iterator &iter)
{
	*this = iter;
}

template<typename K, typename V, class Comparator>
SkipList<K, V,Comparator>::Iterator::Iterator(SkipList<K, V,Comparator>* list)
:list_(list)
{
}

template<typename K, typename V, class Comparator>
SkipList<K, V,Comparator>::Iterator::~Iterator(void)
{

}

template<typename K, typename V, class Comparator>
K& SkipList<K, V,Comparator>::Iterator::key()
{
	if (list_ == nullptr || node_ == nullptr || *this == list_->end()) {
		throw std::runtime_error("SkipList<K, V,Comparator>::Iterator::operator*(): Iterator not pointer any SkipList node.");
	}
	return node_->key;
}

template<typename K, typename V, class Comparator>
V& SkipList<K, V,Comparator>::Iterator::value()
{
	if (list_ == nullptr || node_ == nullptr || *this == list_->end()) {
		throw std::runtime_error("SkipList<K, V,Comparator>::Iterator::operator*(): Iterator not pointer any SkipList node.");
	}
	return node_->value;
}

template<typename K, typename V, class Comparator>
bool SkipList<K, V,Comparator>::Iterator::operator==(const SkipList<K, V,Comparator>::Iterator& rhs) const
{
	if (list_ == nullptr || node_ == nullptr) {
		return false;
	}

	if (list_ == rhs.list_ && node_ == rhs.node_) {
		return true;
	}

	return false;
}

template<typename K, typename V, class Comparator>
bool SkipList<K, V,Comparator>::Iterator::operator!=(const SkipList<K, V,Comparator>::Iterator& rhs) const
{
	return !(*this == rhs);
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V,Comparator>::Iterator& SkipList<K, V,Comparator>::Iterator::operator=(const SkipList<K, V,Comparator>::Iterator& rhs)
{
	list_ = rhs.list_;
	node_ = rhs.node_;

	return *this;
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V,Comparator>::Iterator& SkipList<K, V,Comparator>::Iterator::operator++()
{
	if (list_ == nullptr || node_ == nullptr || *this == list_->end()) {
		throw std::runtime_error("SkipList<K, V,Comparator>::Iterator::operator++(): Iterator not pointer any SkipList node.");
	}
	node_ = node_->next;
	return *this;
}

template<typename K, typename V, class Comparator>
typename SkipList<K, V,Comparator>::Iterator SkipList<K, V,Comparator>::Iterator::operator++(int32_t)
{
	if (list_ == nullptr || node_ == nullptr || *this == list_->end()) {
		throw std::runtime_error("SkipList<K, V,Comparator>::Iterator::operator++(int32_t): Iterator not pointer any SkipList node.");
	}
	Iterator tmp = *this;
	node_ = node_->next;

	return tmp;
}

}
#endif