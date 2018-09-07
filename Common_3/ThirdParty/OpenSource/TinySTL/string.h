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

#ifndef TINYSTL_STRING_H
#define TINYSTL_STRING_H

#include "allocator.h"
#include "vector.h"
#include "stddef.h"
#include "hash.h"


 // For memcpy
#include <string.h>
// For sprintf: contains vsnprintf
#include <stdio.h>
// For sprintf: contains va_start, etc.
#include <stdarg.h>
#include <ctype.h>
#if defined(__linux__)
// For ptrdiff_t, etc
#include <stddef.h>
#endif

// disable the unsecare function warning on vsnprintf and sprintf with Microsoft compiler
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

namespace tinystl {
	class string {
	public:
		typedef char* iterator;

		string();
		string(const string& other);
		string(const char* sz);
		string(const char* sz, size_t len);
		~string();

		string& operator=(const string& other);

		operator const char*() const { return m_first; }
		const char& at(size_t index) const { return m_first[index]; }
		char& at(size_t index) { return m_first[index]; }

		iterator begin() const { return m_first; }
		iterator end() const { return m_last; }

		const char* c_str() const;
		size_t size() const;

		void reserve(size_t size);
		void resize(size_t size);

		void append(const char* first, const char* last);
		void push_back(char c);

		void swap(string& other);

		string substring(unsigned pos) const;
		string substring(unsigned pos, unsigned length) const;

		unsigned find(char c, unsigned startPos, bool caseSensitive = true) const;
		unsigned find(const string& str, unsigned startPos, bool caseSensitive = true) const;
		bool rfind(const char ch, int pos = -1, unsigned int* index = nullptr) const;

		unsigned find_last(char c, unsigned startPos = npos, bool caseSensitive = true) const;
		unsigned find_last(const string& str, unsigned startPos = npos, bool caseSensitive = true) const;

		void replace(char replaceThis, char replaceWith, bool caseSensitive = true);
		void replace(const string& replaceThis, const string& replaceWith, bool caseSensitive = true);

		bool insert(const unsigned int pos, const char *string, const unsigned int len);

		string replaced(char s, char r) const;
		string replaced(const string& replaceThis, const string& replaceWith, bool caseSensitive = true) const;

		string trimmed() const;

		string to_lower() const;
		string to_upper() const;

		vector<string> split(char separator, bool keepEmptyStrings = false) const;

		/// Copy chars from one buffer to another.
		static inline void copy_chars(char* dest, const char* src, unsigned count);
		static inline int compare(const char* lhs, const char* rhs, bool caseSensitive);
		static inline string format(const char* fmt, ...);

		/// Position for "not found."
		static const unsigned npos = 0xffffffff;

	private:
		/// Move a range of characters within the string.
		inline void move_range(unsigned dest, unsigned src, unsigned count)
		{
			if (count)
				memmove(m_first + dest, m_first + src, count);
		}


		typedef char* pointer;
		pointer m_first;
		pointer m_last;
		pointer m_capacity;

		static const size_t c_nbuffer = 12;
		char m_buffer[12];
	};

	inline string::string()
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		resize(0);
	}

	inline string::string(const string& other)
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		reserve(other.size());
		append(other.m_first, other.m_last);
	}

	inline string::string(const char* sz)
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		size_t len = 0;
		char temp = '\0';
		if (!sz)
		{
			sz = &temp;
		}
		for (const char* it = sz; *it; ++it)
			++len;

		reserve(len);
		append(sz, sz + len);
	}

	inline string::string(const char* sz, size_t len)
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		reserve(len);
		append(sz, sz + len);
	}

	inline string::~string() {
		if (m_first != m_buffer)
			TINYSTL_ALLOCATOR::static_deallocate(m_first, m_capacity - m_first);
	}

	inline string& string::operator=(const string& other) {
		string(other).swap(*this);
		return *this;
	}

	inline const char* string::c_str() const {
		return m_first;
	}

	inline size_t string::size() const
	{
		return (size_t)(m_last - m_first);
	}

	inline void string::reserve(size_t capacity) {
		if (m_first + capacity + 1 <= m_capacity)
			return;

		const size_t size = (size_t)(m_last - m_first);

		pointer newfirst = (pointer)TINYSTL_ALLOCATOR::static_allocate(capacity + 1);
		for (pointer it = m_first, newit = newfirst, end = m_last; it != end; ++it, ++newit)
			*newit = *it;
		if (m_first != m_buffer)
			TINYSTL_ALLOCATOR::static_deallocate(m_first, m_capacity - m_first);

		m_first = newfirst;
		m_last = newfirst + size;
		m_capacity = m_first + capacity;
	}

	inline void string::resize(size_t size) {
		reserve(size);
		for (pointer it = m_last, end = m_first + size + 1; it < end; ++it)
			*it = 0;

		m_last = m_first + size;
		m_first[size] = '\0';
	}

	inline void string::append(const char* first, const char* last) {
		const size_t newsize = (size_t)((m_last - m_first) + (last - first) + 1);
		if (m_first + newsize > m_capacity)
			reserve((newsize * 3) / 2);

		for (; first != last; ++m_last, ++first)
			*m_last = *first;
		*m_last = 0;
	}

	inline void string::push_back(char c)
	{
		append(&c, (&c) + 1);
	}

	inline void string::swap(string& other) {
		const pointer tfirst = m_first, tlast = m_last, tcapacity = m_capacity;
		m_first = other.m_first, m_last = other.m_last, m_capacity = other.m_capacity;
		other.m_first = tfirst, other.m_last = tlast, other.m_capacity = tcapacity;

		char tbuffer[c_nbuffer];

		if (m_first == other.m_buffer)
			for (pointer it = other.m_buffer, end = m_last, out = tbuffer; it != end; ++it, ++out)
				*out = *it;

		if (other.m_first == m_buffer) {
			other.m_last = other.m_last - other.m_first + other.m_buffer;
			other.m_first = other.m_buffer;
			other.m_capacity = other.m_buffer + c_nbuffer;

			for (pointer it = other.m_first, end = other.m_last, in = m_buffer; it != end; ++it, ++in)
				*it = *in;
			*other.m_last = 0;
		}

		if (m_first == other.m_buffer) {
			m_last = m_last - m_first + m_buffer;
			m_first = m_buffer;
			m_capacity = m_buffer + c_nbuffer;

			for (pointer it = m_first, end = m_last, in = tbuffer; it != end; ++it, ++in)
				*it = *in;
			*m_last = 0;
		}
	}

	inline string string::substring(unsigned pos) const
	{
		if (pos < (unsigned)size())
		{
			string ret;
			ret.resize((unsigned)size() - pos);
			copy_chars(ret.m_first, m_first + pos, (unsigned)ret.size());

			return ret;
		}
		else
			return string();
	}

	inline string string::substring(unsigned pos, unsigned length) const
	{
		if (pos < (unsigned)size())
		{
			string ret;
			if (pos + length > (unsigned)size())
				length = (unsigned)size() - pos;
			ret.resize(length);
			copy_chars(ret.m_first, m_first + pos, (unsigned)ret.size());

			return ret;
		}
		else
			return string();
	}

	inline unsigned string::find(char c, unsigned startPos, bool caseSensitive /* = true*/) const
	{
		if (caseSensitive)
		{
			for (unsigned i = startPos; i < (unsigned)size(); ++i)
			{
				if (m_first[i] == c)
					return i;
			}
		}
		else
		{
			c = (char)tolower(c);
			for (unsigned i = startPos; i < (unsigned)size(); ++i)
			{
				if (tolower(m_first[i]) == c)
					return i;
			}
		}

		return npos;
	}

	inline unsigned string::find(const string& str, unsigned startPos, bool caseSensitive /* = true*/) const
	{
		if (!(unsigned)str.size() || (unsigned)str.size() > (unsigned)size())
			return npos;

		char first = str.m_first[0];
		if (!caseSensitive)
			first = (char)tolower(first);

		for (unsigned i = startPos; i <= (unsigned)size() - (unsigned)str.size(); ++i)
		{
			char c = m_first[i];
			if (!caseSensitive)
				c = (char)tolower(c);

			if (c == first)
			{
				unsigned skip = npos;
				bool found = true;
				for (unsigned j = 1; j < (unsigned)str.size(); ++j)
				{
					c = m_first[i + j];
					char d = str.m_first[j];
					if (!caseSensitive)
					{
						c = (char)tolower(c);
						d = (char)tolower(d);
					}

					if (skip == npos && c == first)
						skip = i + j - 1;

					if (c != d)
					{
						found = false;
						if (skip != npos)
							i = skip;
						break;
					}
				}
				if (found)
					return i;
			}
		}

		return npos;
	}

	inline bool string::rfind(const char ch, int pos, unsigned int* index) const
	{
		unsigned int i = (pos < 0) ? static_cast<unsigned int>(size()) : pos;

		while (i)
		{
			i--;
			if (m_first[i] == ch)
			{
				if (index != nullptr)
				{
					*index = i;
					return true;
				}
			}
		}

		return false;
	}

	inline unsigned string::find_last(char c, unsigned startPos /* = npos*/, bool caseSensitive /* = true*/) const
	{
		if (startPos >= (unsigned)size())
			startPos = (unsigned)size() - 1;

		if (caseSensitive)
		{
			for (unsigned i = startPos; i < (unsigned)size(); --i)
			{
				if (m_first[i] == c)
					return i;
			}
		}
		else
		{
			c = (char)tolower(c);
			for (unsigned i = startPos; i < (unsigned)size(); --i)
			{
				if (tolower(m_first[i]) == c)
					return i;
			}
		}

		return npos;
	}

	inline unsigned string::find_last(const string& str, unsigned startPos /* = npos*/, bool caseSensitive /* = true*/) const
	{
		if (!(unsigned)str.size() || (unsigned)str.size() > (unsigned)size())
			return npos;
		if (startPos > (unsigned)size() - (unsigned)str.size())
			startPos = (unsigned)size() - (unsigned)str.size();

		char first = *str.m_first;
		if (!caseSensitive)
			first = (char)tolower(first);

		for (unsigned i = startPos; i < (unsigned)size(); --i)
		{
			char c = m_first[i];
			if (!caseSensitive)
				c = (char)tolower(c);

			if (c == first)
			{
				bool found = true;
				for (unsigned j = 1; j < (unsigned)str.size(); ++j)
				{
					c = m_first[i + j];
					char d = str.m_first[j];
					if (!caseSensitive)
					{
						c = (char)tolower(c);
						d = (char)tolower(d);
					}

					if (c != d)
					{
						found = false;
						break;
					}
				}
				if (found)
					return i;
			}
		}

		return npos;
	}

	inline void string::replace(char replaceThis, char replaceWith, bool caseSensitive /* = true*/)
	{
		if (caseSensitive)
		{
			for (unsigned i = 0; i < (unsigned)size(); ++i)
			{
				if (m_first[i] == replaceThis)
					m_first[i] = replaceWith;
			}
		}
		else
		{
			replaceThis = (char)tolower(replaceThis);
			for (unsigned i = 0; i < (unsigned)size(); ++i)
			{
				if (tolower(m_first[i]) == replaceThis)
					m_first[i] = replaceWith;
			}
		}
	}

	inline void string::replace(const string& replaceThis, const string& replaceWith, bool caseSensitive /* = true*/)
	{
		unsigned nextPos = 0;

		while (nextPos < (unsigned)size())
		{
			unsigned pos = find(replaceThis, nextPos, caseSensitive);
			if (pos == npos)
				break;

			unsigned srcLength = (unsigned)replaceWith.size();
			unsigned length = (unsigned)replaceThis.size();
			const char* srcStart = replaceWith.c_str();
			int delta = (int)srcLength - (int)length;

			if (pos + length < (unsigned)size())
			{
				if (delta < 0)
				{
					move_range(pos + srcLength, pos + length, (unsigned)size() - pos - length);
					resize((unsigned)size() + delta);
				}
				if (delta > 0)
				{
					resize((unsigned)size() + delta);
					move_range(pos + srcLength, pos + length, (unsigned)size() - pos - length - delta);
				}
			}
			else
				resize((unsigned)size() + delta);

			copy_chars(m_first + pos, srcStart, srcLength);


			replace(pos, (unsigned)replaceThis.size(), replaceWith);
			nextPos = pos + (unsigned)replaceWith.size();
		}
	}

	inline bool string::insert(const unsigned int pos, const char *string, const unsigned int len)
	{
		if (pos > size())
			return false;

		size_t length = size();
		size_t newLength = length + len;

		resize(newLength);

		size_t n = length - pos;
		for (unsigned int i = 0; i <= n; i++)
			m_first[newLength - i] = m_first[length - i];

		strncpy(m_first + pos, string, len);

		return true;
	}

	inline string string::replaced(char s, char r) const
	{
		string str = *this;
		str.replace(s, r);
		return str;
	}

	inline string string::replaced(const string& replaceThis, const string& replaceWith, bool caseSensitive /* = true*/) const
	{
		string ret = *this;
		ret.replace(replaceThis, replaceWith, caseSensitive);
		return ret;
	}

	inline string string::trimmed() const
	{
		unsigned trimStart = 0;
		unsigned trimEnd = (unsigned)size();

		while (trimStart < trimEnd)
		{
			char c = m_first[trimStart];
			if (c != ' ' && c != 9)
				break;
			++trimStart;
		}
		while (trimEnd > trimStart)
		{
			char c = m_first[trimEnd - 1];
			if (c != ' ' && c != 9)
				break;
			--trimEnd;
		}

		return substring(trimStart, trimEnd - trimStart);
	}

	inline string string::to_lower() const
	{
		string ret = *this;
		for (unsigned i = 0; i < (unsigned)ret.size(); ++i)
			ret.m_first[i] = (char)tolower(m_first[i]);

		return ret;
	}

	inline string string::to_upper() const
	{
		string ret = *this;
		for (unsigned i = 0; i < (unsigned)ret.size(); ++i)
			ret.m_first[i] = (char)toupper(m_first[i]);

		return ret;
	}

	inline vector<string> string::split(char separator, bool keepEmptyStrings /* = false*/) const
	{
		const char* str = c_str();
		vector<string> ret;
		const char* strEnd = str + strlen(str);

		for (const char* splitEnd = str; splitEnd != strEnd; ++splitEnd)
		{
			if (*splitEnd == separator)
			{
				const ptrdiff_t splitLen = splitEnd - str;
				if (splitLen > 0 || keepEmptyStrings)
					ret.push_back(string(str, splitLen));
				str = splitEnd + 1;
			}
		}

		const ptrdiff_t splitLen = strEnd - str;
		if (splitLen > 0 || keepEmptyStrings)
			ret.push_back(string(str, splitLen));

		return ret;
	}

	inline bool operator==(const string& lhs, const string& rhs) {

		const size_t lsize = lhs.size(), rsize = rhs.size();
		if (lsize != rsize)
			return false;

		// use memcmp - this is usually an intrinsic on most compilers
		return memcmp(lhs.c_str(), rhs.c_str(), lsize) == 0;
	}

	inline bool operator==(const string& lhs, const char* rhs) {

		const size_t lsize = lhs.size(), rsize = strlen(rhs);
		if (lsize != rsize)
			return false;

		return memcmp(lhs.c_str(), rhs, lsize) == 0;
	}

	inline bool operator<(const string& lhs, const string& rhs) {

		const size_t lsize = lhs.size(), rsize = rhs.size();
		if (lsize != rsize)
			return lsize < rsize;
		return memcmp(lhs.c_str(), rhs.c_str(), lsize) < 0;
	}

	inline bool operator>(const string& lhs, const string& rhs) {

		const size_t lsize = lhs.size(), rsize = rhs.size();
		if (lsize != rsize)
			return lsize > rsize;
		return memcmp(lhs.c_str(), rhs.c_str(), lsize) > 0;
	}

	inline bool operator<=(const string& lhs, const string& rhs) {
		return !(lhs > rhs);
	}

	inline bool operator>=(const string& lhs, const string& rhs) {
		return !(lhs < rhs);
	}

	inline bool operator!=(const string& lhs, const string& rhs) {
		return !(lhs == rhs);
	}

	inline bool operator!=(const string& lhs, const char* rhs) {
		return !(lhs == rhs);
	}

	inline bool operator!=(const char* lhs, const string& rhs) {
		return !(rhs == lhs);
	}

	inline string operator+(const string& lhs, const string& rhs) {
		string ret(lhs);
		ret.append(rhs.begin(), rhs.end());
		return ret;
	}

	inline string& operator+=(string& lhs, const string& rhs) {
		lhs.append(rhs.begin(), rhs.end());
		return lhs;
	}

	static inline unsigned int hash(const string& value) {
		return hash_string(value.c_str(), value.size());
	}

	inline void string::copy_chars(char* dest, const char* src, unsigned count)
	{
#ifdef _MSC_VER
		if (count)
			memcpy(dest, src, count);
#else
		char* end = dest + count;
		while (dest != end)
		{
			*dest = *src;
			++dest;
			++src;
		}
#endif
	}

	inline int string::compare(const char* lhs, const char* rhs, bool caseSensitive)
	{
		if (!lhs || !rhs)
			return lhs ? 1 : (rhs ? -1 : 0);

		if (caseSensitive)
			return strcmp(lhs, rhs);
		else
		{
			for (;;)
			{
				char l = (char)tolower(*lhs);
				char r = (char)tolower(*rhs);
				if (!l || !r)
					return l ? 1 : (r ? -1 : 0);
				if (l < r)
					return -1;
				if (l > r)
					return 1;

				++lhs;
				++rhs;
			}
		}
	}

	inline string string::format(const char* fmt, ...)
	{
		int size = int(strlen(fmt) * 2 + 50);
		string str;
		va_list ap;
		while (1) {     // Maximum two passes on a POSIX system...
			str.resize(size);
			va_start(ap, fmt);
			int n = vsnprintf((char *)str.c_str(), size, fmt, ap);
			va_end(ap);
			if (n > -1 && n < size) {  // Everything worked
				str.resize(n);
				return str;
			}
			if (n > -1)  // Needed size returned
				size = n + 1;   // For null char
			else
				size *= 2;      // Guess at a larger size (OS specific)
		}
		return str;
	}
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
