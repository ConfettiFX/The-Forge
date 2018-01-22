#pragma once

#include "vector.h"

namespace tinystl
{
	template< class T>
	class resource_collection
	{
	public:
		~resource_collection()
		{
			// cleanup remaining resources
			for (auto& arrayObject : m_array)
				delete arrayObject;

			// force size 0
			m_array.resize(0);
		}
		size_t create()
		{
			if (m_freelist.empty())
			{
				m_array.push_back(new T());
				return m_array.size();
			}
			else
			{
				size_t idx = m_freelist.back();
				m_freelist.pop_back();
				m_array[idx] = new T();
				return ++idx;
			}
		}
		void destroy(size_t idx)
		{
			delete m_array[--idx];
			m_array[idx] = nullptr;
			m_freelist.push_back(idx);
		}

		T& operator [] (size_t idx)
		{
			return *m_array[--idx];
		}

		size_t find(T* ptr)
		{
			for (size_t i = 0; i < m_array.size(); ++i)
			{
				if (m_array[i] == ptr)
					return i + 1;
			}

			return 0;
		}

	protected:
		tinystl::vector<T*> m_array;
		tinystl::vector<size_t> m_freelist;
	};
};