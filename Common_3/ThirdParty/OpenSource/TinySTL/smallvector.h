#ifndef SMALL_VECTOR_H
#define SMALL_VECTOR_H

#include "allocator.h"
#include "buffer.h"


#include "../../../OS/Core/DLL.h"

/* smallvector is a class aimed to have the same functionality as a regular vector, but avoiding to dinamically allocate memory every time it reaches its capacity.
** It does this by using a statically sized array of S size (known at compile-time), when the number of elements is less than S, and a buffer
** in which dinamically stores all the extra elements that it needs and don't fit in the array.
** Because of this, this class is best used whenever you have a realistic estimate of how many elements you'll have inside the vector at almost all times. This way, you'll not
** allocate anything at runtime unless you really need it.
** The class supports all the functionality in tinystl::vector, even all the iterator-related functions (thanks to the use of a smart iterator that knows where both the array and buffer are in memory).
*/

namespace tinystl {
	// Smart iterator (needed to loop coherently through the array and buffer).
	template<typename T>
	struct CONFETTI_DLLAPI smallVectorIterator
	{
		T* p;
		T* pArray;
		T* pBuffer;
		size_t arraySize;
	public:
		smallVectorIterator(T* x, T* y, T* z, size_t w) :p(x), pArray(y), pBuffer(z), arraySize(w) {}
		smallVectorIterator(const T* x, const T* y, const T* z, const size_t w) : p(const_cast<T*>(x)), pArray(const_cast<T*>(y)), pBuffer(const_cast<T*>(z)), arraySize(w) {}
		smallVectorIterator(const smallVectorIterator<T>& mit) : p(mit.p), pArray(mit.pArray), pBuffer(mit.pBuffer), arraySize(mit.arraySize) {}

		smallVectorIterator<T>& operator++() {
			if (p == pArray + arraySize - 1) p = pBuffer;
			else ++p;
			return *this;
		}
		smallVectorIterator<T>& operator--() {
			if (p == pBuffer) p = pArray + arraySize - 1;
			else --p;
			return *this;
		}
		smallVectorIterator<T> operator++(int) { smallVectorIterator<T> tmp(*this); operator++(); return tmp; }
		smallVectorIterator<T> operator--(int) { smallVectorIterator<T> tmp(*this); operator--(); return tmp; }

		T& operator*() { return *p; }
		const T& operator*() const { return *p; }
		T* operator&() { return p; }
		const T* operator&() const { return p; }

		bool operator==(const smallVectorIterator<T>& rhs) { return p == rhs.p; }
		bool operator!=(const smallVectorIterator<T>& rhs) { return p != rhs.p; }
		const bool operator==(const smallVectorIterator<T>& rhs) const { return p == rhs.p; }
		const bool operator!=(const smallVectorIterator<T>& rhs) const { return p != rhs.p; }


		smallVectorIterator<T>& operator+=(const int& i) {
			size_t offset = p - pArray;
			if (offset + i >= arraySize && p + i < pBuffer) p = pBuffer + i - (arraySize - offset);
			else p += i;
			return *this;
		}
		smallVectorIterator<T> operator+(const int& i)
		{
			smallVectorIterator<T> tmp(*this);
			operator+=(i);
			return tmp;
		}

		smallVectorIterator<T>& operator-=(const int& i) {
			size_t offset = p - pBuffer;
			if (p - i < pBuffer && p - pArray >= arraySize) p = pArray + arraySize - i + offset;
			else p -= i;
			return *this;
		}
		smallVectorIterator<T> operator-(const int& i)
		{
			smallVectorIterator<T> tmp(*this);
			operator-=(i);
			return tmp;
		}
	};

	template<typename T, size_t S, typename Alloc = TINYSTL_ALLOCATOR>
	class CONFETTI_DLLAPI smallvector {
	public:
		smallvector();
		smallvector(const smallvector& other);
		smallvector(const T& value);
		~smallvector();

		smallvector& operator=(const smallvector& other);

		void assign(const T* first, const T* last);

		const T* array_data() const;
		T* array_data();
		const T* buffer_data() const;
		T* buffer_data();
		size_t array_size() const;
		size_t array_capacity() const;
		size_t buffer_size() const;
		size_t buffer_capacity() const;
		size_t size() const;
		bool empty() const;

		T& operator[](size_t idx);
		const T& operator[](size_t idx) const;

		const T& front() const;
		T& front();
		const T& back() const;
		T& back();

		void resize(size_t size);
		void resize(size_t size, const T& value);
		void clear();
		void reserve(size_t capacity);

		void push_back(const T& t);
		void pop_back();

		void emplace_back();
		template<typename Param>
		void emplace_back(const Param& param);

		void shrink_to_fit();

		void swap(smallvector& other);

		smallVectorIterator<T> begin();
		smallVectorIterator<T> end();

		const smallVectorIterator<T> begin() const;
		const smallVectorIterator<T> end() const;

		void insert(T* where);
		void insert(T* where, const T& value);
		void insert(T* where, const T* first, const T* last);

		template<typename Param>
		void emplace(T* where, const Param& param);

		T* erase(T* where);
		T* erase(T* first, T* last);

		T* erase_unordered(T* where);
		T* erase_unordered(T* first, T* last);
/*
		// [Confetti backwards compatibility]
		T* getArray() const { return m_array; }
		T* abandonArray();
		unsigned int getCount() const { return static_cast<unsigned int>(size()); }
		void setCount(const unsigned int newCount);
		const unsigned int add(const T& t);
		void remove(const unsigned int index);
		void orderedRemove(const unsigned int index);
		void fastRemove(const unsigned int index);
		void reset();
*/
	private:
		int partition(int(*compare)(const T &elem0, const T &elem1), int p, int r);
		void quickSort(int(*compare)(const T &elem0, const T &elem1), int p, int r);

	public:
		void sort(int(*compare)(const T &elem0, const T &elem1));

	private:
		T m_array[S];
		T* m_arrayIdx;
		buffer<T, Alloc> m_buffer;
	};

	template<typename T, size_t S, typename Alloc>
	inline smallvector<T, S, Alloc>::smallvector(){
		m_arrayIdx = m_array;
		buffer_init(&m_buffer);
	}

	template<typename T, size_t S, typename Alloc>
	inline smallvector<T, S, Alloc>::smallvector(const smallvector& other) {
		for (m_arrayIdx = m_array; m_arrayIdx < m_array + other.array_size(); ++m_arrayIdx)
			*m_arrayIdx = other.m_array[m_arrayIdx - m_array];
		buffer_init(&m_buffer);
		buffer_reserve(&m_buffer, other.buffer_size());
		buffer_insert(&m_buffer, m_buffer.last, other.m_buffer.first, other.m_buffer.last);
	}

	template<typename T, size_t S, typename Alloc>
	inline smallvector<T, S, Alloc>::smallvector(const T& value) {
		for(m_arrayIdx = m_array; m_arrayIdx < m_array + S; ++m_arrayIdx)
			*m_arrayIdx = value;
		buffer_init(&m_buffer);
	}

	template<typename T, size_t S, typename Alloc>
	inline smallvector<T, S, Alloc>::~smallvector() {
		m_arrayIdx = nullptr;
		buffer_destroy(&m_buffer);
	}

	template<typename T, size_t S, typename Alloc>
	inline smallvector<T, S, Alloc>& smallvector<T, S, Alloc>::operator=(const smallvector& other){
		smallvector(other).swap(*this);
		return *this;
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::assign(const T* first, const T* last){
		const size_t size = last - first;
		for (m_arrayIdx = m_array; m_arrayIdx < m_array + min(size, S); ++m_arrayIdx)
			*m_arrayIdx = *(first + array_size());
		if(size > S){
			buffer_clear(&m_buffer);
			buffer_insert(&m_buffer, m_buffer.last, first + S, last);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline const T* smallvector<T, S, Alloc>::array_data() const {
		return m_array;
	}

	template<typename T, size_t S, typename Alloc>
	inline T* smallvector<T, S, Alloc>::array_data(){
		return m_array;
	}

	template<typename T, size_t S, typename Alloc>
	inline const T* smallvector<T, S, Alloc>::buffer_data() const {
		return m_buffer.first;
	}

	template<typename T, size_t S, typename Alloc>
	inline T* smallvector<T, S, Alloc>::buffer_data(){
		return m_buffer.first;
	}

	template<typename T, size_t S, typename Alloc>
	inline size_t smallvector<T, S, Alloc>::array_size() const {
		return m_arrayIdx - m_array;
	}

	template<typename T, size_t S, typename Alloc>
	inline size_t smallvector<T, S, Alloc>::array_capacity() const {
		return S;
	}

	template<typename T, size_t S, typename Alloc>
	inline size_t smallvector<T, S, Alloc>::buffer_size() const {
		return m_buffer.last - m_buffer.first;
	}

	template<typename T, size_t S, typename Alloc>
	inline size_t smallvector<T, S, Alloc>::buffer_capacity() const {
		return m_buffer.capacity - m_buffer.first;
	}

	template<typename T, size_t S, typename Alloc>
	inline size_t smallvector<T, S, Alloc>::size() const {
		return array_size() + buffer_size();
	}

	template<typename T, size_t S, typename Alloc>
	inline bool smallvector<T, S, Alloc>::empty() const {
		return m_arrayIdx == m_array;
	}

	template<typename T, size_t S, typename Alloc>
	inline T& smallvector<T, S, Alloc>::operator[](size_t idx){
		if (idx < S) {
			assert(idx < array_size() && "smallvector[]: index is out of range."); // Maybe even remove it.
			return m_array[idx];
		}
		assert(idx >= size() && "smallvector[]: index is out of range."); // Maybe even remove it.
		return m_buffer.first[idx - S];
	}

	template<typename T, size_t S, typename Alloc>
	inline const T& smallvector<T, S, Alloc>::operator[](size_t idx) const {
		if (idx < S) {
			assert(idx < array_size() && "smallvector[]: index is out of range."); // Maybe even remove it.
			return m_array[idx];
		}
		assert(idx >= size() && "smallvector[]: index is out of range."); // Maybe even remove it.
		return m_buffer.first[idx - S];
	}

	template<typename T, size_t S, typename Alloc>
	inline T& smallvector<T, S, Alloc>::front(){
		return m_array[0];
	}

	template<typename T, size_t S, typename Alloc>
	inline const T& smallvector<T, S, Alloc>::front() const {
		return m_array[0];
	}

	template<typename T, size_t S, typename Alloc>
	inline T& smallvector<T, S, Alloc>::back(){
		if(buffer_size() == 0) return *m_arrayIdx;
		else return m_buffer.last[-1];
	}

	template<typename T, size_t S, typename Alloc>
	inline const T& smallvector<T, S, Alloc>::back() const {
		if(buffer_size() == 0) return *m_arrayIdx;
		else return m_buffer.last[-1];
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::resize(size_t size){
		// How do I control negative size? (-1 = 18446744073709551615)
		if (size < array_size()){
			m_arrayIdx = m_array + size;
			buffer_destroy(&m_buffer);
		} else if (size > S){
			if (buffer_size() == 0) buffer_reserve(&m_buffer, size - S);
			else buffer_resize(&m_buffer, size - S);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::resize(size_t size, const T& value){
		// How do I control negative size? (-1 = 18446744073709551615)
		if (size < array_size()) {
			m_arrayIdx = m_array + size;
			buffer_destroy(&m_buffer);
		}
		else if (size > S) {
			if (buffer_size() == 0) buffer_reserve(&m_buffer, size - S);
			else buffer_resize(&m_buffer, size - S);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::clear() {
		m_arrayIdx = m_array;
		buffer_clear(&m_buffer);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::reserve(size_t capacity) {
		if (capacity > S) buffer_reserve(&m_buffer, capacity - S);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::push_back(const T& t) {
		if(array_size() >= S) buffer_append(&m_buffer, &t);
		else {*m_arrayIdx = t; ++m_arrayIdx;}
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::pop_back() {
		if(buffer_size() != 0) buffer_erase(&m_buffer, m_buffer.last - 1, m_buffer.last);
		else m_arrayIdx = (m_arrayIdx == m_array) ? m_array : --m_arrayIdx;
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::emplace_back() {
		if (array_size() >= S) buffer_append(&m_buffer);
		else { *m_arrayIdx = NULL; ++m_arrayIdx; }
	}

	template<typename T, size_t S, typename Alloc>
	template<typename Param>
	inline void smallvector<T, S, Alloc>::emplace_back(const Param& param) {
		if (array_size() >= S) buffer_append(&m_buffer, &param);
		else { *m_arrayIdx = param; ++m_arrayIdx; }
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::shrink_to_fit() {
		if(size() <= S) buffer_init(&m_buffer); // Breaks if with empty buffers if I don't do this check.
		else buffer_shrink_to_fit(&m_buffer);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::swap(smallvector& other) {
		for(size_t i = 0; i < array_size(); ++i){
			const size_t aux = m_array[i];
			m_array[i] = other.m_array[i];
			other.m_array[i] = aux;
		}
		other.m_arrayIdx = other.m_array + array_size();
		buffer_swap(&m_buffer, &other.m_buffer);
	}

	template<typename T, size_t S, typename Alloc>
	inline smallVectorIterator<T> smallvector<T, S, Alloc>::begin() {
		return smallVectorIterator<T>(m_array, m_array, m_buffer.first, S);
	}

	template<typename T, size_t S, typename Alloc>
	inline smallVectorIterator<T> smallvector<T, S, Alloc>::end() {
		if (buffer_size() == 0) return smallVectorIterator<T>(m_arrayIdx, m_array, m_buffer.first, S);
		else return smallVectorIterator<T>(m_buffer.last , m_array, m_buffer.first, S);
	}

	template<typename T, size_t S, typename Alloc>
	inline const smallVectorIterator<T> smallvector<T, S, Alloc>::begin() const {
		return smallVectorIterator<T>(m_array, m_array, m_buffer.first, S);
	}

	template<typename T, size_t S, typename Alloc>
	inline const smallVectorIterator<T> smallvector<T, S, Alloc>::end() const {
		if (buffer_size() == 0) return smallVectorIterator<T>(m_arrayIdx, m_array, m_buffer.first, S);
		else return smallVectorIterator<T>(m_buffer.last , m_array, m_buffer.first, S);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::insert(T* where) {
		size_t offset = where - m_array;
		if (offset == S) buffer_insert(&m_buffer, m_buffer.first, 1);
		else if(offset > S) buffer_insert(&m_buffer, where, 1);
		else {
			if(S - array_size() < 1) buffer_insert(&m_buffer, m_buffer.first, &m_array[S - 1], &m_array[S]);

			T previousValue = NULL;
			for (; offset < min(array_size() + 1, S); ++offset)
			{
				const size_t aux = m_array[offset];
				m_array[offset] = previousValue;
				previousValue = aux;
			}

			m_arrayIdx = min(m_array + S, ++m_arrayIdx);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::insert(T* where, const T& value) {
		size_t offset = where - m_array;
		if (offset == S) buffer_insert(&m_buffer, m_buffer.first, &value, &value + 1);
		else if (offset > S) buffer_insert(&m_buffer, where, &value, &value + 1);
		else {
			if (S - array_size() < 1) buffer_insert(&m_buffer, m_buffer.first, &m_array[S - 1], &m_array[S]);

			T previousValue = value;
			for (; offset < min(array_size() + 1, S); ++offset)
			{
				const size_t aux = m_array[offset];
				m_array[offset] = previousValue;
				previousValue = aux;
			}

			m_arrayIdx = min(m_array + S, ++m_arrayIdx);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::insert(T* where, const T* first, const T* last) {
		size_t offset = where - m_array;
		if (offset == S) buffer_insert(&m_buffer, m_buffer.first, first, last);
		else if (offset > S) buffer_insert(&m_buffer, where, first, last);
		else {
			const size_t availableArraySize = S - array_size();
			const size_t count = last - first;

			if (availableArraySize < count) {
				buffer_insert(&m_buffer, m_buffer.first, m_array + offset, m_arrayIdx);
				buffer_insert(&m_buffer, m_buffer.first, last - offset, last);
			}

			size_t valueOffset = (count - 1) % S - offset;
			offset = min(offset + count * 2 - 1, S - 1);
			for (; offset > 0; --offset)
			{
				m_array[offset] = *(where + valueOffset);
				if (offset == count) valueOffset = last - where - 1;
				else --valueOffset;
			}

			m_arrayIdx = min(m_array + S, m_arrayIdx + count);
		}
	}

	template<typename T, size_t S, typename Alloc>
	template<typename Param>
	void smallvector<T, S, Alloc>::emplace(T* where, const Param& param) {
		size_t offset = where - m_array;
		if (offset == S) buffer_insert(&m_buffer, m_buffer.first, &param, &param + 1);
		else if (offset > S) buffer_insert(&m_buffer, where, &param, &param + 1);
		else {
			if (S - array_size() < 1) buffer_insert(&m_buffer, m_buffer.first, &m_array[S - 1], &m_array[S]);

			T previousValue = param;
			for (; offset < min(array_size() + 1, S); ++offset)
			{
				const size_t aux = m_array[offset];
				m_array[offset] = previousValue;
				previousValue = aux;
			}

			m_arrayIdx = min(m_array + S, ++m_arrayIdx);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline T* smallvector<T, S, Alloc>::erase(T* where) {
		size_t offset = where - m_array;
		if (offset == S) buffer_erase(&m_buffer, m_buffer.first, m_buffer.first + 1);
		else if (offset > S) return buffer_erase(&m_buffer, where, where + 1);
		else {
			for (; offset < array_size() - 1; ++offset)
				m_array[offset] = m_array[offset + 1];
			if (size() > S) {
				m_array[S - 1] = *m_buffer.first;
				buffer_erase(&m_buffer, m_buffer.first, m_buffer.first + 1);
			} else --m_arrayIdx;
			return where;
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline T* smallvector<T, S, Alloc>::erase(T* first, T* last) {
		size_t offset = first - m_array;
		const size_t count = last - first;

		if (offset == S) buffer_erase(&m_buffer, m_buffer.first, last);
		else if (offset > S) return buffer_erase(&m_buffer, first, last);
		else {
			if (size() <= S) {
				if (count > array_size()) m_arrayIdx = 0;
				else {
					for (; offset < array_size() - count; ++offset)
						m_array[offset] = m_array[offset + count];
					m_arrayIdx -= count;
				}
				return first;
			}
			else {
				const size_t bufferCount = last - m_buffer.first;

				size_t valueOffset;
				for (; offset <= buffer_size() - bufferCount; ++offset) {
					valueOffset = offset - (first - m_array);
					m_array[offset] = *(m_buffer.first + (bufferCount + valueOffset));
				}
				m_arrayIdx = m_array + offset + valueOffset;

				m_array[offset] = *m_buffer.first;
				buffer_erase(&m_buffer, m_buffer.first, m_buffer.first + bufferCount + 1);
				return first;
			}
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline T* smallvector<T, S, Alloc>::erase_unordered(T* where) {
		size_t offset = where - m_array;
		if (offset == S) buffer_erase_unordered(&m_buffer, m_buffer.first, m_buffer.first + 1);
		else if (offset > S) return buffer_erase_unordered(&m_buffer, where, where + 1);
		else {
			for (; offset < array_size() - 1; ++offset)
				m_array[offset] = m_array[offset + 1];
			if (size() > S) {
				m_array[S - 1] = *m_buffer.first;
				buffer_erase_unordered(&m_buffer, m_buffer.first, m_buffer.first + 1);
			}
			else --m_arrayIdx;
			return where;
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline T* smallvector<T, S, Alloc>::erase_unordered(T* first, T* last) {
		size_t offset = first - m_array;
		const size_t count = last - first;

		if (offset == S) buffer_erase_unordered(&m_buffer, m_buffer.first, last);
		else if (offset > S) return buffer_erase_unordered(&m_buffer, first, last);
		else {
			if (size() <= S) {
				if (count > array_size()) m_arrayIdx = 0;
				else {
					for (; offset < array_size() - count; ++offset)
						m_array[offset] = m_array[offset + count];
					m_arrayIdx -= count;
				}
				return first;
			}
			else {
				const size_t bufferCount = last - m_buffer.first;

				size_t valueOffset;
				for (; offset <= buffer_size() - bufferCount; ++offset) {
					valueOffset = offset - (first - m_array);
					m_array[offset] = *(m_buffer.first + (bufferCount + valueOffset));
				}
				m_arrayIdx = m_array + offset + valueOffset;

				m_array[offset] = *m_buffer.first;
				buffer_erase_unordered(&m_buffer, m_buffer.first, m_buffer.first + bufferCount + 1);
				return first;
			}
		}
	}

	template<typename T, size_t S, typename Alloc>
	T* smallvector<T, S, Alloc>::abandonArray()
	{
		T* r = m_array;
		m_arrayIdx = m_array;
		buffer_init(&m_buffer);
		return r;
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::setCount(const unsigned int newCount)
	{
		resize(newCount);
	}

	template<typename T, size_t S, typename Alloc>
	inline const unsigned int smallvector<T, S, Alloc>::add(const T& t)
	{
		push_back(t);
		return static_cast<unsigned int>(size() - 1);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::fastRemove(const unsigned int index)
	{
		// Fast remove used to cause a memory leak in the old stl
		// Just remove regularly instead
		erase(m_array + index);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::remove(const unsigned int index)
	{
		erase(m_array + index);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::orderedRemove(const unsigned int index)
	{
		erase(m_array + index);
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::reset()
	{
		clear();
	}

	template<typename T, size_t S, typename Alloc>
	inline int smallvector<T, S, Alloc>::partition(int(*compare)(const T &elem0, const T &elem1), int p, int r)
	{
		T tmp, pivot = (*this)[p];
		int left = p;

		for (int i = p + 1; i <= r; i++) {
			if (compare((*this)[i], pivot) < 0) {
				left++;
				tmp = (*this)[i];
				(*this)[i] = (*this)[left];
				(*this)[left] = tmp;
			}
		}
		tmp = (*this)[p];
		(*this)[p] = (*this)[left];
		(*this)[left] = tmp;
		return left;
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::quickSort(int(*compare)(const T &elem0, const T &elem1), int p, int r)
	{
		if (p < r) {
			int q = partition(compare, p, r);
			quickSort(compare, p, q - 1);
			quickSort(compare, q + 1, r);
		}
	}

	template<typename T, size_t S, typename Alloc>
	inline void smallvector<T, S, Alloc>::sort(int(*compare)(const T &elem0, const T &elem1))
	{
		quickSort(compare, 0, (int)size() - 1);
	}
}

#endif // !SMALL_VECTOR_H
