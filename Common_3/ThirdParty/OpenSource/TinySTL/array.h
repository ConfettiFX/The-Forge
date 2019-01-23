#ifndef __TINYSTL_ARRAY__
#define __TINYSTL_ARRAY__

#include <stdexcept>
#include <iterator>

namespace tinystl {
	template <typename T, size_t N>
	class array {
	public:
		/*** 1. Element Access ***/

		// access specified elements with bounds checking
		T &at(size_t pos)
		{
			if (pos < 0 || pos >= N)
				throw std::out_of_range("index out of range");
			return data[pos];
		}

		const T &at(size_t pos) const
		{
			if (pos < 0 || pos >= N)
				throw std::out_of_range("index out of range");
			return data[pos];
		}

		// access specified elements
		T &operator [](size_t pos) { return data[pos]; }

		// access first element
		T &front()
		{
			if (this->empty())
				throw std::out_of_range("empty array");
			return data[0];
		}

		const T &front() const
		{
			if (this->empty())
				throw std::out_of_range("empty array");
			return data[0];
		}

		// access last element
		T &back()
		{
			if (this->empty())
				throw std::out_of_range("empty array");
			return data[N - 1];
		}

		const T &back() const
		{
			if (this->empty())
				throw std::out_of_range("empty array");
			return data[N - 1];
		}

		/*** 2. Iterator ***/

		class Iterator : public std::iterator<std::forward_iterator_tag, array<T, N>>
		{
		public:
			bool operator ==(const Iterator &I) { return (this->curr == I.curr); }
			bool operator !=(const Iterator &I) { return (this->curr != I.curr); }

			T &operator *() { return *curr; }

			Iterator operator ++() { return Iterator(++curr); }
			Iterator operator ++(int dummy) { return Iterator(curr++); }

			Iterator(T *_curr = nullptr) : curr(_curr) {}

		private:
			T *curr;

			friend class array<T, N>;
		};

		class ConstIterator : public std::iterator<std::forward_iterator_tag, array<T, N>>
		{
		public:
			bool operator ==(const ConstIterator &I) { return (this->curr == I.curr); }
			bool operator !=(const ConstIterator &I) { return (this->curr != I.curr); }

			const T &operator *() { return *curr; }

			ConstIterator operator ++() { return ConstIterator(++curr); }
			ConstIterator operator ++(int dummy) { return ConstIterator(curr++); }

			ConstIterator(const T *_curr = nullptr) : curr(_curr) {}

		private:
			const T *curr;

			friend class array<T, N>;
		};

		class ReverseIterator : public std::iterator<std::forward_iterator_tag, array<T, N>>
		{
		public:
			bool operator ==(const ReverseIterator &I) { return (this->curr == I.curr); }
			bool operator !=(const ReverseIterator &I) { return (this->curr != I.curr); }

			T &operator *() { return *curr; }

			ReverseIterator operator ++() { return ReverseIterator(--curr); }
			ReverseIterator operator ++(int dummy) { return ReverseIterator(curr--); }

			ReverseIterator(T *_curr = nullptr) : curr(_curr) {}

		private:
			T *curr;

			friend class array<T, N>;
		};

		// iterator to the beginning
		Iterator begin() { return Iterator(this->empty() ? nullptr : &this->front()); }

		// iterator to the end
		Iterator end() { return Iterator(this->empty() ? nullptr : &(this->back()) + 1); }

		// const iterator to the beginning
		ConstIterator cbegin() const { return ConstIterator(this->empty() ? nullptr : &this->front()); }

		// const iterator to the end
		ConstIterator cend() const { return ConstIterator(this->empty() ? nullptr : &(this->back()) + 1); }

		// reverse iterator to the beginning
		ReverseIterator rbegin() { return ReverseIterator(this->empty() ? nullptr : &this->back()); }

		// reverse iterator to the end
		ReverseIterator rend() { return ReverseIterator(this->empty() ? nullptr : &(this->front()) - 1); }

		/*** 3. Capacity ***/

		// checks whether the array is empty
		constexpr bool empty() const { return (N == 0); }

		// returns the number of elements
		constexpr size_t size() const { return N; }

		/*** 4. Operations ***/

		// fill the array with given value
		void fill(const T &val) {
			for (size_t i = 0; i < N; ++i)
				this->data[i] = val;
		}

		// swap content with another array
		void swap(array<T, N> &other) {
			for (size_t i = 0; i < N; ++i)
				swap(this->data[i], other.data[i]);
		}

	private:
		T data[N];

		friend class Iterator;
		friend class ReverseIterator;
	};

}

namespace std
{
	template< size_t I, class T, size_t N >
	constexpr T& get(tinystl::array<T, N>& a) noexcept
	{
		return a.at(I);
	}
}

#endif