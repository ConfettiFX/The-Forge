/*-
 * Copyright 2012-2015 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TINYSTL_MEMORY_H
#define TINYSTL_MEMORY_H

#include "Detail/Ref.h"
#include "allocator.h"

namespace tinystl
{
	//Forward decls
	template<class T, class D>
	class unique_ptr;
	
	template <class T, class D>
	void swap(unique_ptr<T, D>& x, unique_ptr<T, D>& y);
	
	//////
	// TODO: reformat header to conform.
	// UNIQUE PTR
	//////
	template<class T>
	struct default_delete {
		void operator ()(T* ptr) 
		{ 
			if (ptr)
			{
				ptr->~T();
				allocator::static_deallocate(ptr, sizeof(T));
			}
		}
	};

	template<class T>
	struct default_delete < T[] > 
	{
		void operator ()(T* ptr) 
		{ 
			if (ptr)
			{
				//@remark : Not sure how we want to handle calling the destructor for the whole array!
				allocator::static_deallocate(ptr);
			}
		}
	};

	template<class T, class D = default_delete<T>>
	class unique_ptr {
	public:
		typedef T element_type;
		typedef D deleter_type;
		typedef element_type *pointer;
	public:
		explicit unique_ptr(T *data = nullptr) :data_(data) {}
		unique_ptr(T *data, deleter_type del) :data_(data), deleter(del) {}

		unique_ptr(unique_ptr&& up) :data_(nullptr) {
			tinystl::swap(data_, up.data_);
		}
		unique_ptr& operator = (unique_ptr&& up) {
			if (&up != this) {
				clean();
				tinystl::swap(*this, up);
			}
			return *this;
		}

		//Derived type pointer
		template<typename T2>
		unique_ptr(unique_ptr<T2>&& up) :data_(up.release())
		{
			static_assert(std::is_base_of<T, T2>::value, "Cannot move pointer of non-derived type.");
		}

		unique_ptr(const unique_ptr&) = delete;
		unique_ptr& operator = (const unique_ptr&) = delete;

		~unique_ptr() { clean(); }

		const pointer get()const { return data_; }
		pointer get() { return data_; }
		deleter_type& get_deleter() { return deleter; }
		const deleter_type& get_deleter()const { return deleter; }

		operator bool()const { return get() != nullptr; }

		pointer release() {
			T *p = data_;
			data_ = nullptr;
			return p;
		}
		void reset(pointer p = pointer()) {
			clean();
			data_ = p;
		}
		void swap(unique_ptr& up) 
		{ 
			T* p = data_;
			data_ = up.data_;
			up.data_ = p;
		}

		template<typename U = element_type>
		const typename std::enable_if_t<!std::is_void_v<U>, U>& operator *()const { return *data_; }
		const pointer operator ->()const { return data_; }
		template<typename U = element_type>
		typename std::enable_if_t<!std::is_void_v<U>, U>& operator *() { return *data_; }
		pointer operator ->() { return data_; }
	private:
		inline void clean() {
			deleter(data_);
			data_ = nullptr;
		}
	private:
		element_type *data_;
		deleter_type deleter;
	};
	template <class T, class D>
	void swap(unique_ptr<T, D>& x, unique_ptr<T, D>& y) {
		x.swap(y);
	}
	template <class T1, class D1, class T2, class D2>
	bool operator == (const unique_ptr<T1, D1>& lhs, const unique_ptr<T2, D2>& rhs) {
		return lhs.get() == rhs.get();
	}
	template <class T, class D>
	bool operator == (const unique_ptr<T, D>& up, nullptr_t p) {
		return up.get() == p;
	}
	template <class T, class D>
	bool operator == (nullptr_t p, const unique_ptr<T, D>& up) {
		return up.get() == p;
	}
	template <class T1, class D1, class T2, class D2>
	bool operator != (const unique_ptr<T1, D1>& lhs, const unique_ptr<T2, D2>& rhs) {
		return !(lhs == rhs);
	}
	template <class T, class D>
	bool operator != (const unique_ptr<T, D>& up, nullptr_t p) {
		return up.get() != p;
	}
	template <class T, class D>
	bool operator != (nullptr_t p, const unique_ptr<T, D>& up) {
		return up.get() != p;
	}

	template <class T, class... Args>
	unique_ptr<T> make_unique(Args&&... args) {
		T* ptr = (T*)allocator::static_allocate(sizeof(T));
		ptr = new(ptr) T(std::forward<Args>(args)...);
		return unique_ptr<T>(ptr);
	};

	//////
	// TODO: reformat header to conform.
	// SHARED PTR
	//////
	template<class T>
	class shared_ptr {
	public:
		typedef T element_type;
	private:
		template<class Type>
		using ref_t = Detail::ref_t < Type >;
	public:
		explicit shared_ptr(T *p = nullptr) :ref_(new ref_t<T>(p)) {}
		template<class D>
		shared_ptr(T *p, D del) : ref_(new ref_t<T>(p, del)) {}

		shared_ptr(const shared_ptr& sp) {
			copy_ref(sp.ref_);
		}

		shared_ptr& operator = (const shared_ptr& sp) {
			if (this != &sp) {
				decrease_ref();
				copy_ref(sp.ref_);
			}
			return *this;
		}

		~shared_ptr() { decrease_ref(); }

		const element_type& operator *()const { return *(get()); }
		const element_type *operator ->()const { return get(); }
		element_type& operator *() { return *(get()); }
		element_type *operator ->() { return get(); }

		const element_type* get() const { return ref_->get_data(); }
		element_type* get() { return ref_->get_data(); }
		size_t use_count() const { return ref_->count(); }

		operator bool() const { return get() != nullptr; }
	private:
		void decrease_ref() {
			if (ref_->get_data()) {
				--(*ref_);
				if (use_count() == 0)
				{
					ref_->~ref_t<T>();
					allocator::static_deallocate(ref_);
				}
			}
		}
		void copy_ref(ref_t<T> *r) {//ÕýÈ·µÄ¿½±´ref_t
			ref_ = r;
			++(*ref_);
		}
	private:
		ref_t<T> *ref_;

	public:
		template<class _T>
		friend class cow_ptr;
	};
	template<class T1, class T2>
	bool operator == (const shared_ptr<T1>& lhs, const shared_ptr<T2>& rhs) {
		return lhs.get() == rhs.get();
	}
	template<class T>
	bool operator == (const shared_ptr<T>& sp, nullptr_t p) {
		return sp.get() == p;
	}
	template<class T>
	bool operator == (nullptr_t p, const shared_ptr<T>& sp) {
		return sp == p;
	}
	template<class T1, class T2>
	bool operator != (const shared_ptr<T1>& lhs, const shared_ptr<T2>& rhs) {
		return !(lhs == rhs);
	}
	template<class T>
	bool operator != (const shared_ptr<T>& sp, nullptr_t p) {
		return !(sp == p);
	}
	template<class T>
	bool operator != (nullptr_t p, const shared_ptr<T>& sp) {
		return !(sp == p);
	}

	template<class T, class...Args>
	shared_ptr<T> make_shared(Args... args) {
		T* ptr = (T*)allocator::static_allocate(sizeof(T));
		ptr = new(ptr) T(std::forward<Args>(args)...);
		return shared_ptr<T>(ptr);
	}
	
	//////////////////////////////
	// WEAK PTR
	//////////////////////////////
	// template class weak_ptr
	template<typename T>
	class weak_ptr {
	private:
		// original pointer
		T* __ptr;
		// reference count
		unsigned int* __countptr;
	public:
		friend class shared_ptr<T>;
		// constructor
		weak_ptr() : __ptr(nullptr), __countptr(new unsigned int) {}
		weak_ptr(const shared_ptr<T>& ptr): __ptr(ptr.__ptr), __countptr(ptr.__countptr) {}
		weak_ptr(const weak_ptr<T>& ptr): __ptr(ptr.__ptr), __countptr(ptr.__countptr) {}
		weak_ptr(weak_ptr<T>&& ptr) noexcept : __ptr(ptr.__ptr), __countptr(ptr.__countptr) 
		{
			ptr.__ptr = nullptr;
			ptr.__countptr = nullptr;
		}
		// assignment operator
		weak_ptr<T>& operator= (const weak_ptr<T>& ptr) 
		{
			if (this != &ptr) {
				__ptr = ptr.__ptr;
				__countptr = ptr.__countptr;
			}
			return *this;
		}
		weak_ptr<T>& operator= (const shared_ptr<T>& ptr) 
		{
			__ptr = ptr.__ptr;
			__countptr = ptr.__countptr;
			return *this;
		}
		weak_ptr<T>& operator= (weak_ptr<T>&& ptr) noexcept 
		{
			if (this != &ptr) {
				__ptr = ptr.__ptr;
				__countptr = ptr.__countptr;
				ptr.__ptr = nullptr;
				ptr.__countptr = nullptr;
			}
			return *this;
		}

		// deconstructor
		~weak_ptr() = default;

		// some frequently used functions
		void reset() 
		{
			__ptr = nullptr;
			__countptr = nullptr;
		}

		unsigned int use_count() 
		{
			return *__countptr;
		}

		bool expired() 
		{
			if (__countptr == nullptr) return true;
			return use_count() == 0;
		}

		shared_ptr<T> lock() 
		{
			return shared_ptr<T>(*this);
		}

		void swap(weak_ptr<T>& ptr) noexcept 
		{
			weak_ptr<T> tmp = *this;
			*this = ptr;
			ptr = tmp;
		}
	};
}

#endif