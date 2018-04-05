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

#ifndef TINYSTL_STRINGHASH_H
#define TINYSTL_STRINGHASH_H

#include "stddef.h"
#include <math.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include "../Nothings/stb_hash.h"

#ifdef __APPLE__
#define __forceinline inline
#endif

namespace tinystl
{
	static inline unsigned int hash_string(const char* str, size_t len) {
#ifdef _MSC_VER
		return crc32c_hw(str, (int)len, 0);
#else

		// Implementation of sdbm a public domain string hash from Ozan Yigit
		// see: http://www.eecs.harvard.edu/margo/papers/usenix91/paper.ps

		unsigned int hash = 0;
		typedef const char* pointer;
		for (pointer it = str, end = str + len; it != end; ++it)
			hash = *it + (hash << 6) + (hash << 16) - hash;

		return hash;
#endif
	}

	static inline unsigned int hash(const char* str) {
		const char* strEnd = str;
		while (*strEnd != '\0') { ++strEnd; }
		return hash_string(str, strEnd - str);
	}

	template<typename T>
	inline unsigned int hash(const T& value) {
		return hash_string((const char*)&value, sizeof(value));
	}

#if defined(__linux__)
#define __forceinline __attribute__((always_inline))
#endif

	template <typename T> static __forceinline T align_up_with_mask(T value, uint64_t mask)
	{
		return (T)(((uint64_t)value + mask) & ~mask);
	}

	template <typename T> static __forceinline T align_down_with_mask(T value, uint64_t mask)
	{
		return (T)((uint64_t)value & ~mask);
	}

	template <typename T> static __forceinline T align_up(T value, uint64_t alignment)
	{
		return align_up_with_mask(value, alignment - 1);
	}

	template <typename T> static __forceinline T align_down(T value, uint64_t alignment)
	{
		return align_down_with_mask(value, alignment - 1);
	}

#ifdef _M_X64
#define ENABLE_SSE_CRC32 1
#else
#define ENABLE_SSE_CRC32 0
#endif

#if ENABLE_SSE_CRC32
#pragma intrinsic(_mm_crc32_u32)
#pragma intrinsic(_mm_crc32_u64)
#endif

	static inline uint64_t hash_range(const uint32_t* const Begin, const uint32_t* const End, uint64_t Hash)
	{
#if ENABLE_SSE_CRC32
		const uint64_t* Iter64 = (const uint64_t*)align_up(Begin, 8);
		const uint64_t* const End64 = (const uint64_t* const)align_down(End, 8);

		// If not 64-bit aligned, start with a single u32
		if ((uint32_t*)Iter64 > Begin)
			Hash = _mm_crc32_u32((uint32_t)Hash, *Begin);

		// Iterate over consecutive u64 values
		while (Iter64 < End64)
			Hash = _mm_crc32_u64((uint64_t)Hash, *Iter64++);

		// If there is a 32-bit remainder, accumulate that
		if ((uint32_t*)Iter64 < End)
			Hash = _mm_crc32_u32((uint32_t)Hash, *(uint32_t*)Iter64);
#else
		// An inexpensive hash for CPUs lacking SSE4.2
		for (const uint32_t* Iter = Begin; Iter < End; ++Iter)
			Hash = 16777619U * Hash ^ *Iter;
#endif

		return Hash;
	}

	template <typename T> static inline uint64_t hash_state(const T* StateDesc, uint64_t Count = 1, uint64_t Hash = 2166136261U)
	{
		static_assert((sizeof(T) & 3) == 0 && alignof(T) >= 4, "State object is not word-aligned");
		return hash_range((uint32_t*)StateDesc, (uint32_t*)(StateDesc + Count), Hash);
	}

}

#include <cstdlib> //malloc
#include <cstring> //memset

struct HashEntry
{
	unsigned int *value;
	HashEntry *next;
	unsigned int index;
};

class Hash
{
public:
	Hash(const unsigned int dim, const unsigned int entryCount, const unsigned int capasity)
	{
		curr = mem = (unsigned char*)malloc(entryCount * sizeof(HashEntry *) + capasity * (sizeof(HashEntry) + sizeof(unsigned int) * dim));

		nDim = dim;
		count = 0;
		nEntries = entryCount;
		entries = (HashEntry **)newMem(entryCount * sizeof(HashEntry *));

		memset(entries, 0, entryCount * sizeof(HashEntry *));
	}

	~Hash() {
		free(mem);
	}

	bool insert(const unsigned int *value, unsigned int *index) {
		unsigned int hash = 0;//0xB3F05C27;
		unsigned int i = 0;
		do {
			hash += value[i];
			hash += (hash << 11);
			//hash ^= (hash >> 6);
			i++;
		} while (i < nDim);

		hash %= nEntries;

		HashEntry *entry = entries[hash];

		while (entry) {
			if (memcmp(value, entry->value, nDim * sizeof(unsigned int)) == 0) {
				*index = entry->index;
				return true;
			}

			entry = entry->next;
		}

		HashEntry *newEntry = (HashEntry *)newMem(sizeof(HashEntry));
		newEntry->value = (unsigned int *)newMem(sizeof(unsigned int) * nDim);

		memcpy(newEntry->value, value, nDim * sizeof(unsigned int));
		newEntry->index = count++;

		newEntry->next = entries[hash];
		entries[hash] = newEntry;

		*index = newEntry->index;
		return false;
	}

	static size_t hash(const char* val)
	{
		return tinystl::hash(val);
	}

	static size_t hash(const char* str, unsigned int len)
	{
		return tinystl::hash_string(str, len);
	}

	unsigned int getCount() const { return count; }

protected:
	unsigned int nDim;
	unsigned int count;
	unsigned int nEntries;

	HashEntry **entries;

	void *newMem(const unsigned int size) {
		unsigned char *rmem = curr;
		curr += size;
		return rmem;
	}

	unsigned char *mem, *curr;
};

#endif
