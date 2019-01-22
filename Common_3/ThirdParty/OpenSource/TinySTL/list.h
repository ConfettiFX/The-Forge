#ifndef __TINYSTL_LIST__
#define __TINYSTL_LIST__

#include <stdexcept>
#include "allocator.h"
#include "utils.h"

namespace tinystl {
	template <typename U>
	struct list_node {
		U *pval;
		list_node *prev, *next;

		list_node() : pval(nullptr), prev(nullptr), next(nullptr) {}
	};

	template<typename T>
	struct DefaultListAllocator
	{
		template<typename ... Args>
		T* allocate_and_construct(size_t count, Args... args)
		{
			T* ptr = (T*)allocator::static_allocate(count * sizeof(T));
			for (size_t i = 0; i < count; ++i)
			{
				ptr[i] = conf_placement_new(ptr[i], args...);
			}
			return ptr;
		}

		void destroy_and_deallocate(T* ptr, size_t count)
		{
			for (size_t i = 0; i < count; ++i)
			{
				ptr[i].~T();
			}
			allocator::static_deallocate(ptr);
		}
	};

	template <typename T, class Alloc = DefaultListAllocator<T>>
	class list {
	public:
		/*** 1. Element Access ***/

		// access first element
		T &front() {
			if (head == nullptr)
				throw std::out_of_range("empty list");
			return *(head->pval);
		}

		// access last element
		T &back() {
			if (tail == nullptr)
				throw std::out_of_range("empty list");
			return *(tail->pval);
		}

		/*** 2. Iterator ***/

		class Iterator : public ForwardIterator {
		public:
			bool operator ==(const Iterator &I) { return (this->curr == I.curr); }
			bool operator !=(const Iterator &I) { return (this->curr != I.curr); }

			T &operator *() { return *(curr->pval); }

			Iterator operator ++() {
				advance();
				return Iterator(this->curr);
			}

			Iterator operator ++(int dummy) {
				Iterator temp(this->curr);
				advance();
				return temp;
			}

			Iterator(list_node<T> *_curr = nullptr) : curr(_curr) {}

		private:
			void advance() { curr = curr->next; }

			list_node<T> *curr;

			friend class list<T>;
		};

		class ReverseIterator : public BackwardIterator {
		public:
			bool operator ==(const ReverseIterator &I) { return (this->curr == I.curr); }
			bool operator !=(const ReverseIterator &I) { return (this->curr != I.curr); }

			T &operator *() { return *(curr->pval); }

			ReverseIterator operator ++() {
				advance();
				return ReverseIterator(this->curr);
			}

			ReverseIterator operator ++(int dummy) {
				ReverseIterator temp(this->curr);
				advance();
				return temp;
			}

			ReverseIterator(list_node<T> *_curr = nullptr) : curr(_curr) {}

		private:
			void advance() { curr = curr->prev; }

			list_node<T> *curr;

			friend class list<T>;
		};

		// iterator to the beginning
		Iterator begin() { return Iterator(head); }

		// iterator to the end
		Iterator end() { return Iterator(nullptr); }

		// reverse iterator to the beginning
		ReverseIterator rbegin() { return ReverseIterator(tail); }

		// reverse iterator to the end
		ReverseIterator rend() { return ReverseIterator(nullptr); }

		/*** 3. Capacity ***/

		// checks whether the list is empty
		bool empty() { return (N == 0); }

		// returns the number of elements
		unsigned int size() { return N; }

		/*** 4. Modifiers ***/

		// add element at beginning
		void push_front(const T &val) {
			if (this->empty()) {
				head = createNode(val);
				tail = head;
			}
			else {
				head->prev = createNode(val);
				head->prev->next = head;
				head = head->prev;
			}
			++N;
		}

		// delete first element
		void pop_front() {
			if (head == nullptr)
				throw std::out_of_range("empty list");
			if (N == 1) {
				deleteNode(head);
				head = tail = nullptr;
			}
			else {
				auto temp = head;
				head = head->next;
				head->prev = nullptr;
				deleteNode(temp);
			}
			--N;
		}

		// add element at end
		void push_back(const T &val) {
			if (this->empty()) {
				head = createNode(val);
				tail = head;
			}
			else {
				tail->next = createNode(val);
				tail->next->prev = tail;
				tail = tail->next;
			}
			++N;
		}

		// delete last element
		void pop_back() {
			if (tail == nullptr)
				throw std::out_of_range("empty list");
			if (N == 1) {
				deleteNode(head);
				head = tail = nullptr;
			}
			else {
				auto temp = tail;
				tail = tail->prev;
				tail->next = nullptr;
				deleteNode(temp);
				--N;
			}
		}

		// insert elements
		void insert(Iterator pos, const T &val) {
			if (pos == this->begin()) {
				push_front(val);
			}
			else if (pos == this->end()) {
				push_back(val);
			}
			else {
				list_node<T> *pivot = pos.curr;
				list_node<T> *new_node = createNode(val);
				pivot->prev->next = new_node;
				new_node->prev = pivot->prev;
				new_node->next = pivot;
				pivot->prev = new_node;
				++N;
			}
		}

		// erase elements
		Iterator erase(Iterator pos) {
			if (pos == this->end())
				return pos;
			if (pos == this->begin()) {
				pop_front();
				return this->begin();
			}
			else {
				list_node<T> *pivot = pos.curr;
				pivot->prev->next = pivot->next;
				pivot->next->prev = pivot->prev;
				pos = Iterator(pivot->next);
				deleteNode(pivot);
				--N;
				return pos;
			}
		}

		// swap content with another list
		void swap(list<T> &other) {
			swap(this->head, other.head);
			swap(this->tail, other.tail);
			swap(this->N, other.N);
		}

		// clear content
		void clear() {
			for (; N > 0; --N) {
				auto rest = head->next;
				deleteNode(head);
				head = rest;
			}
			head = tail = nullptr;
		}

		/*** 5. Constructor and Destructor ***/

		// constructor
		list() : head(nullptr), tail(nullptr), N(0) {}

		// destructor
		~list() { clear(); }

	private:
		list_node<T> *head, *tail;
		unsigned int N;
		Alloc alloc;

		template <class ...Args>
		list_node<T> *createNode(Args... args) {
			list_node<T> *p = (list_node<T> *)allocator::static_allocate(sizeof(list_node<T>));
			p = conf_placement_new(p);
			p->pval = alloc.allocate_and_construct(1, args...);
			return p;
		}

		void deleteNode(list_node<T> *p) {
			alloc.destroy_and_deallocate(p->pval, 1);
			allocator::static_deallocate(p);
		}

		friend class Iterator;
		friend class ReverseIterator;
	};
};

#endif