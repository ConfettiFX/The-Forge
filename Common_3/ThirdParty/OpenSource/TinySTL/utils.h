#ifndef __TINYSTL_UTILS__
#define __TINYSTL_UTILS__

#include <functional>

namespace tinystl
{
	/*** Compare ***/
	template <typename T>
	bool less(const T &a, const T &b) { return a < b; };

	template <typename T>
	bool greater(const T &a, const T &b) { return a > b; }

	template <typename T>
	bool equal(const T &a, const T &b) { return a == b; }

	// decltype(Less<T>) = bool (*cmp)(const T &, const T &)
	template <typename T>
	T min(const T &a, const T &b, decltype(less<T>) cmp = less<T>) { return cmp(a, b) ? a : b; }

	template <typename T>
	T max(const T &a, const T &b, decltype(less<T>) cmp = less<T>) { return cmp(a, b) ? b : a; }

	/*** Iterator Tag ***/
	class ForwardIterator {};
	class BackwardIterator {};

	template <typename T, typename Iterator>
	Iterator find(Iterator begin, Iterator end, const T &val, bool(*pred)(const T &a, const T &b) = equal<T>) {
		for (; begin != end; ++begin)
			if (pred(*begin, val))
				break;
		return begin;
	}

	template <typename T, typename Iterator>
	unsigned int count(Iterator begin, Iterator end, const T &val, bool(*pred)(const T &a, const T &b) = equal<T>) {
		unsigned int count = 0;
		for (; begin != end; ++begin)
			if (pred(*begin, val))
				++count;
		return count;
	}
};

#endif