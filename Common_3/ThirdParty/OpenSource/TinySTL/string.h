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

		// [Confetti backwards compatibility]
		char& operator [] (unsigned int n);
		const char& operator [] (unsigned int n) const;
		unsigned int getLength() const { return (unsigned int)size(); }
		string sprintf(const string* fmt, ...);
		string sprintf(const char* fmt, ...);
		bool isEmpty() const { return size() == 0; }
		bool find(const char ch, int pos = 0, unsigned int* index = nullptr) const;
		bool find(const char *string, unsigned int pos = 0, unsigned int *index = nullptr) const;
		bool rfind(const char ch, int pos = -1, unsigned int* index = nullptr) const;
		bool insert(const unsigned int pos, const char *string, const unsigned int len);
		void append(const char* string, const unsigned int len);
		void appendInt(const int integer);
		void appendLong(const long longInt);
		void appendFloat(const float floatPoint);

		unsigned find(char c, unsigned startPos, bool caseSensitive = true) const
		{
			if (caseSensitive)
			{
				for (unsigned i = startPos; i < getLength(); ++i)
				{
					if (m_first[i] == c)
						return i;
				}
			}
			else
			{
				c = (char)tolower(c);
				for (unsigned i = startPos; i < getLength(); ++i)
				{
					if (tolower(m_first[i]) == c)
						return i;
				}
			}

			return npos;
		}

		unsigned find(const string& str, unsigned startPos, bool caseSensitive = true) const
		{
			if (!str.getLength() || str.getLength() > getLength())
				return npos;

			char first = str.m_first[0];
			if (!caseSensitive)
				first = (char)tolower(first);

			for (unsigned i = startPos; i <= getLength() - str.getLength(); ++i)
			{
				char c = m_first[i];
				if (!caseSensitive)
					c = (char)tolower(c);

				if (c == first)
				{
					unsigned skip = npos;
					bool found = true;
					for (unsigned j = 1; j < str.getLength(); ++j)
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

		void replace(char replaceThis, char replaceWith, bool caseSensitive = true)
		{
			if (caseSensitive)
			{
				for (unsigned i = 0; i < getLength(); ++i)
				{
					if (m_first[i] == replaceThis)
						m_first[i] = replaceWith;
				}
			}
			else
			{
				replaceThis = (char)tolower(replaceThis);
				for (unsigned i = 0; i < getLength(); ++i)
				{
					if (tolower(m_first[i]) == replaceThis)
						m_first[i] = replaceWith;
				}
			}
		}

		void replace(const string& replaceThis, const string& replaceWith, bool caseSensitive = true)
		{
			unsigned nextPos = 0;

			while (nextPos < getLength())
			{
				unsigned pos = find(replaceThis, nextPos, caseSensitive);
				if (pos == npos)
					break;
				replace(pos, replaceThis.getLength(), replaceWith);
				nextPos = pos + replaceWith.getLength();
			}
		}

		iterator replace(const iterator& start, const iterator& e, const string& replaceWith)
		{
			unsigned pos = (unsigned)(start - begin());
			if (pos >= getLength())
				return end();
			unsigned length = (unsigned)(e - start);
			replace(pos, length, replaceWith);

			return begin() + pos;
		}

		void replace(unsigned pos, unsigned length, const string& replaceWith)
		{
			// If substring is illegal, do nothing
			if (pos + length > getLength())
				return;

			replace(pos, length, replaceWith.m_first, replaceWith.getLength());
		}

		void replace(unsigned pos, unsigned length, const char* replaceWith)
		{
			// If substring is illegal, do nothing
			if (pos + length > getLength())
				return;

			replace(pos, length, replaceWith, (unsigned)strlen(replaceWith));
		}


		void replace(unsigned pos, unsigned length, const char* srcStart, unsigned srcLength)
		{
			int delta = (int)srcLength - (int)length;

			if (pos + length < getLength())
			{
				if (delta < 0)
				{
					move_range(pos + srcLength, pos + length, getLength() - pos - length);
					resize(getLength() + delta);
				}
				if (delta > 0)
				{
					resize(getLength() + delta);
					move_range(pos + srcLength, pos + length, getLength() - pos - length - delta);
				}
			}
			else
				resize(getLength() + delta);

			copy_chars(m_first + pos, srcStart, srcLength);
		}

		string replaced(char s, char r) const
		{
			string str = *this;
			str.replace(s, r);
			return str;
		}

		/// Move a range of characters within the string.
		void move_range(unsigned dest, unsigned src, unsigned count)
		{
			if (count)
				memmove(m_first + dest, m_first + src, count);
		}

		void assign(const char* string, const unsigned int len);
		int toInt() const;
		float toFloat() const;
		double toDouble() const;
		void makeLowerCase();

		unsigned find_last(char c, unsigned startPos = npos, bool caseSensitive = true) const
		{
			if (startPos >= getLength())
				startPos = getLength() - 1;

			if (caseSensitive)
			{
				for (unsigned i = startPos; i < getLength(); --i)
				{
					if (m_first[i] == c)
						return i;
				}
			}
			else
			{
				c = (char)tolower(c);
				for (unsigned i = startPos; i < getLength(); --i)
				{
					if (tolower(m_first[i]) == c)
						return i;
				}
			}

			return npos;
		}

		unsigned find_last(const string& str, unsigned startPos = npos, bool caseSensitive = true) const
		{
			if (!str.getLength() || str.getLength() > getLength())
				return npos;
			if (startPos > getLength() - str.getLength())
				startPos = getLength() - str.getLength();

			char first = *str.m_first;
			if (!caseSensitive)
				first = (char)tolower(first);

			for (unsigned i = startPos; i < getLength(); --i)
			{
				char c = m_first[i];
				if (!caseSensitive)
					c = (char)tolower(c);

				if (c == first)
				{
					bool found = true;
					for (unsigned j = 1; j < str.getLength(); ++j)
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

		string substring(unsigned pos) const
		{
			if (pos < getLength())
			{
				string ret;
				ret.resize(getLength() - pos);
				copy_chars(ret.m_first, m_first + pos, ret.getLength());

				return ret;
			}
			else
				return string();
		}

		string substring(unsigned pos, unsigned length) const
		{
			if (pos < getLength())
			{
				string ret;
				if (pos + length > getLength())
					length = getLength() - pos;
				ret.resize(length);
				copy_chars(ret.m_first, m_first + pos, ret.getLength());

				return ret;
			}
			else
				return string();
		}

		string trimmed() const
		{
			unsigned trimStart = 0;
			unsigned trimEnd = getLength();

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

		string to_lower() const
		{
			string ret = *this;
			for (unsigned i = 0; i < ret.getLength(); ++i)
				ret[i] = (char)tolower(m_first[i]);

			return ret;
		}

		string to_upper() const
		{
			string ret = *this;
			for (unsigned i = 0; i < ret.getLength(); ++i)
				ret[i] = (char)toupper(m_first[i]);

			return ret;
		}

		vector<string> split(char separator, bool keepEmptyStrings = false) const
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

		/// Copy chars from one buffer to another.
		static void copy_chars(char* dest, const char* src, unsigned count)
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

		static int compare(const char* lhs, const char* rhs, bool caseSensitive)
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

		/// Position for "not found."
		static const unsigned npos = 0xffffffff;

		// [\Confetti backwards compatibility]
	private:
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

	inline string string::sprintf(const string* fmt, ...)
	{
		int size = ((int)fmt->size()) * 2 + 50;   // Use a rubric appropriate for your code
		string str;
		va_list ap;
		while (1) {     // Maximum two passes on a POSIX system...
			str.resize(size);
			va_start(ap, fmt);
			int n = vsnprintf((char *)str.c_str(), size, fmt->c_str(), ap);
			va_end(ap);
			if (n > -1 && n < size) {  // Everything worked
				str.resize(n);
				this->append(str.c_str(), (const unsigned int)str.size());
				return *this;
			}
			if (n > -1)  // Needed size returned
				size = n + 1;   // For null char
			else
				size *= 2;      // Guess at a larger size (OS specific)
		}
		return str;
	}

	inline string string::sprintf(const char* fmt, ...)
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
				this->append(str.c_str(), (const unsigned int)str.size());
				return *this;
			}
			if (n > -1)  // Needed size returned
				size = n + 1;   // For null char
			else
				size *= 2;      // Guess at a larger size (OS specific)
		}
		return str;
	}

	inline char& string::operator [] (unsigned int n)
	{
		// TODO: assert
		//assert(n < size());
		return m_first[n];
	}

	inline const char& string::operator [] (unsigned int n) const
	{
		// TODO: assert
		//assert(n < capacity);
		return m_first[n];
	}

	inline bool string::find(const char ch, int pos, unsigned int* index) const
	{
		size_t length = size();
		for (unsigned int i = pos; i < length; i++)
		{
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

	inline bool string::find(const char *string, unsigned int pos, unsigned int *index) const
	{
		const char *st = strstr(m_first + pos, string);
		if (st != NULL) {
			if (index != NULL) *index = (unsigned int)(st - m_first);
			return true;
		}
		return false;
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

	inline void string::append(const char* string, const unsigned int len)
	{
		size_t length = size();
		resize(length + len);
		strncpy(m_first + length, string, len);
	}

	inline void string::appendInt(const int integer)
	{
		char s[16];
		append(s, ::sprintf(s, "%d", integer));
	}

	inline void string::appendLong(const long longInt)
	{
		char s[32];
		append(s, ::sprintf(s, "%li", longInt));
	}

	inline void string::appendFloat(const float floatPoint)
	{
		char s[16];
		append(s, ::sprintf(s, "%f", floatPoint));
	}

	inline void string::makeLowerCase()
	{
		pointer s = this->m_first;

		while (s != this->m_last)
		{
			if (*s >= 65 && *s <= 90)
			{
				*s = *s + 32;
			}
			s++;
		}
	}

	inline void string::assign(const char* string, const unsigned int len)
	{
		resize(len);
		strncpy(m_first, string, len);
		m_first[len] = '\0';
	}

	inline int string::toInt() const
	{
		return atoi(m_first);
	}

	inline float string::toFloat() const
	{
		return (float)atof(m_first);
	}

	inline double string::toDouble() const
	{
		return atof(m_first);
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

	static inline size_t hash(const string& value) {
		return hash_string(value.c_str(), value.size());
	}
}

typedef tinystl::string String;

typedef struct StringList {
	uint32_t        count;
	const char**    names;
} StringList;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
