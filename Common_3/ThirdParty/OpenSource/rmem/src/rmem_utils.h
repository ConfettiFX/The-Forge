/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef RMEM_UTILS_H
#define RMEM_UTILS_H

#include "rmem_platform.h"

#include <string.h> // memcpy
#include <wchar.h>	// wcscat_s

namespace rmem {

	inline uint32_t uint32_cnttzl(uint32_t _val)
	{
#if RMEM_COMPILER_MSVC && RMEM_PLATFORM_WINDOWS
		unsigned long index;
		_BitScanForward(&index, _val);
		return index;
#elif RMEM_COMPILER_GCC || RMEM_COMPILER_CLANG
		return _val ? __builtin_ctz(_val) : sizeof(_val) * 8;
#else
		// slow but should almost never be used
		for (int i = 0; i < sizeof(_val) * 8; ++i)
			if (_val & (1 << i))
				return i;
		return 32;
#endif
	}

	static inline uint32_t hashStr(const char* _string)
	{
		int hash = 0;
		uint8_t* p = (uint8_t*)_string;

		while (*p != '\0')
		{
			hash = hash + ((hash) << 5) + *(p) + ((*(p)) << 7);
			p++;
		}
		return ((hash) ^ (hash >> 16)) & 0xffff;
	} 
	
	static inline uintptr_t hashStackTrace(uintptr_t* _backTrace, uint32_t _numEntries)
	{
		uintptr_t hash = 0;
		for (uint32_t i=0; i<_numEntries; ++i)
			hash += _backTrace[i];
		return hash;
	}

	template <typename T>
	static inline void addVarToBuffer(const T& _value, uint8_t* _bufferBase, size_t& _bufferPtr)
	{
		memcpy(&_bufferBase[_bufferPtr], &_value, sizeof(T));
		_bufferPtr += sizeof(T);
	}

	/// Utility function to write data to a buffer
	static inline void addPtrToBuffer(void* ininPtr, uint32_t ininSize, uint8_t* _bufferBase, size_t& _bufferPtr)
	{
		memcpy(&_bufferBase[_bufferPtr], ininPtr, ininSize);
		_bufferPtr += ininSize;
	}

	/// Utility function to write a string to a buffer
	static inline void addStrToBuffer(const char* _string, uint8_t* _bufferBase, size_t& _bufferPtr, uint8_t _xor = 0)
	{
		uint32_t _len = _string ? (uint32_t)strlen(_string) : 0;
		memcpy(&_bufferBase[_bufferPtr],&_len,sizeof(uint32_t));
		_bufferPtr += sizeof(uint32_t);
		if (_string)
		{
			memcpy(&_bufferBase[_bufferPtr],_string,sizeof(char)*_len);
			for (uint32_t i=0; i<sizeof(char)*_len; ++i)
				_bufferBase[_bufferPtr+i] = _bufferBase[_bufferPtr+i] ^ _xor;
			_bufferPtr += _len*sizeof(char);
		}
	}

	static inline void addStrToBuffer(const wchar_t* _string, uint8_t* _bufferBase, size_t& _bufferPtr, uint8_t _xor = 0)
	{
		uint32_t _len = _string ? (uint32_t)wcslen(_string) : 0;
		memcpy(&_bufferBase[_bufferPtr],&_len,sizeof(uint32_t));
		_bufferPtr += sizeof(uint32_t);
		if (_string)
		{
			memcpy(&_bufferBase[_bufferPtr],_string,sizeof(wchar_t)*_len);
			for (uint32_t i=0; i<sizeof(wchar_t)*_len; ++i)
				_bufferBase[_bufferPtr+i] = _bufferBase[_bufferPtr+i] ^ _xor;
			_bufferPtr += _len*sizeof(wchar_t);
		}
	}

} // namespace rmem

#endif // RMEM_UTILS_H
