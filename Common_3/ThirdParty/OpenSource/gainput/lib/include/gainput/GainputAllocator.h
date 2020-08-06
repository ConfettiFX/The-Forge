
#ifndef GAINPUTALLOCATOR_H_
#define GAINPUTALLOCATOR_H_

#define IMEMORY_FROM_HEADER
#include "../../../../../../OS/Interfaces/IMemory.h"

namespace gainput
{

/// Interface used to pass custom allocators to the library.
/**
 * If you want the library to use your custom allocator you should implement this interface.
 * Specifically, you should provide implementations for the Allocate() and Deallocate()
 * functions. All other (template) member functions are simply based on those two functions.
 *
 * \sa DefaultAllocator
 */
class GAINPUT_LIBEXPORT Allocator
{
public:
	enum { DefaultAlign = 0 };

	/// Allocates a number of bytes and returns a pointer to the allocated memory.
	/**
	 * \param size The number of bytes to allocate.
	 * \return A memory block encompassing at least size bytes.
	 */
	virtual void* Allocate(size_t size, size_t align = DefaultAlign) = 0;
	/// Deallocates the given memory.
	/**
	 * \param ptr The memory block to deallocate.
	 */
	virtual void Deallocate(void* ptr) = 0;

	/// An operator new-like function that allocates memory and calls T's constructor.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T>
	T* New()
	{
		return new (Allocate(sizeof(T))) T();
	}

	/// An operator new-like function that allocates memory and calls T's constructor with one parameter.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0>
	T* New(P0& p0)
	{
		return new (Allocate(sizeof(T))) T(p0);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with one parameter.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0>
	T* New(const P0& p0)
	{
		return new (Allocate(sizeof(T))) T(p0);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with two parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1>
	T* New(P0& p0, P1& p1)
	{
		return new (Allocate(sizeof(T))) T(p0, p1);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with two parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1>
	T* New(const P0& p0, P1& p1)
	{
		return new (Allocate(sizeof(T))) T(p0, p1);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with two parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1>
	T* New(P0& p0, const P1& p1)
	{
		return new (Allocate(sizeof(T))) T(p0, p1);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with the given parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1, class P2>
	T* New(P0& p0, const P1& p1, const P2& p2)
	{
		return new (Allocate(sizeof(T))) T(p0, p1, p2);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with the given parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1, class P2>
	T* New(P0& p0, const P1& p1, P2& p2)
	{
		return new (Allocate(sizeof(T))) T(p0, p1, p2);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with the given parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1, class P2, class P3>
	T* New(P0& p0, P1& p1, P2& p2, P3& p3)
	{
		return new (Allocate(sizeof(T))) T(p0, p1, p2, p3);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with the given parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1, class P2, class P3>
	T* New(P0& p0, const P1& p1, P2& p2, P3& p3)
	{
		return new (Allocate(sizeof(T))) T(p0, p1, p2, p3);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with the given parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1, class P2, class P3, class P4>
	T* New(P0& p0, P1& p1, P2& p2, P3& p3, P4& p4)
	{
		return new (Allocate(sizeof(T))) T(p0, p1, p2, p3, p4);
	}

	/// An operator new-like function that allocates memory and calls T's constructor with the given parameters.
	/**
	 * \return A pointer to an initialized instance of T.
	 */
	template <class T, class P0, class P1, class P2, class P3, class P4>
	T* New(P0& p0, const P1& p1, P2& p2, P3& p3, P4& p4)
	{
		return new (Allocate(sizeof(T))) T(p0, p1, p2, p3, p4);
	}

	/// An operator delete-like function that calls ptr's constructor and deallocates the memory.
	/**
	 * \param ptr The object to destruct and deallocate.
	 */
	template <class T>
	void Delete(T* ptr)
	{
		if (ptr)
		{
			ptr->~T();
			Deallocate(ptr);
		}
	}
};

/// The default allocator used by the library.
/**
 * Any allocation/deallocation calls are simply forwarded to \c malloc and \c free. Any
 * requested alignment is ignored.
 */
class GAINPUT_LIBEXPORT DefaultAllocator : public Allocator
{
public:
	void* Allocate(size_t size, size_t /*align*/)
	{
		return tf_malloc(size);
	}

	void Deallocate(void* ptr)
	{
		tf_free(ptr);
	}
};

/// Returns the default instance of the default allocator.
/**
 * \sa DefaultAllocator
 */
GAINPUT_LIBEXPORT DefaultAllocator& GetDefaultAllocator();

template<class K, class V>
class GAINPUT_LIBEXPORT HashMap;

/// An allocator that tracks an allocations that were done
/**
 * Any allocation/deallocation calls are simply forwarded to \c malloc and \c free. Any
 * requested alignment is ignored.
 */
class GAINPUT_LIBEXPORT TrackingAllocator : public Allocator
{
public:
	TrackingAllocator(Allocator& backingAllocator, Allocator& internalAllocator = GetDefaultAllocator());
	~TrackingAllocator();

	void* Allocate(size_t size, size_t align = DefaultAlign);
	void Deallocate(void* ptr);

	size_t GetAllocateCount() const { return allocateCount_; }
	size_t GetDeallocateCount() const { return deallocateCount_; }
	size_t GetAllocatedMemory() const { return allocatedMemory_; }

private:
	Allocator& backingAllocator_;
	Allocator& internalAllocator_;
	HashMap<void*, size_t>* allocations_;
	size_t allocateCount_;
	size_t deallocateCount_;
	size_t allocatedMemory_;
};



}

#endif

