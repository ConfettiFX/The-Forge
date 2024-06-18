/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002-2015, and is covered by the BSD open source
 * license and the GPL. Refer to the accompanying documentation for details
 * on usage and license.
 */

/*
 * bstrlib.c
 *
 * This file is the core module for implementing the bstring functions.
 */

#include "../../../../Application/Config.h"

#include "../Nothings/stb_ds.h"

#include <wchar.h>
#include <stdbool.h>

#if defined (_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
# define _CRT_SECURE_NO_WARNINGS
#endif


#ifdef AUTOMATED_TESTING
// This variable disables actual assertions for testing purposes
// Should be initialized to true for bstrlib tests
bool gIsBstrlibTest = false;
#define BSTR_TEST_ASSERT_RET(x, ret)  { if (gIsBstrlibTest && !(x)) return ret;}
#else
#define BSTR_TEST_ASSERT_RET(x, ret) (void)0
#endif
#define BSTR_TEST_ASSERT(x)           BSTR_TEST_ASSERT_RET(x, BSTR_ASSERT_ERR)

// Options
#define BSTRLIB_AGGRESSIVE_MEMORY_FOR_SPEED_TRADEOFF
#define BSTR_ALLOW_USAGE_AFTER_DESTROY

#define BSTR_TMP_BUFFER_SIZE 256

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>

#include "bstrlib.h"

#include "../../../Interfaces/ILog.h"
#include "../../../Interfaces/IMemory.h"

/* Optionally include a mechanism for debugging memory */

#if defined(MEMORY_DEBUG) || defined(BSTRLIB_MEMORY_DEBUG)
#include "memdbg.h"
#endif

#ifdef ENABLE_MEMORY_TRACKING
#undef BSTR_CALL
#define BSTR_CALL(fn, ...) fn##Impl(FILE_NAME, FILE_LINE, FUNCTION_NAME, PARENT_FUNCTION_NAME, __VA_ARGS__)
#define BSTR_CALL_NO_TRACKING(fn, ...) fn##Impl(__FILE__, __LINE__, __FUNCTION__, "", __VA_ARGS__)
#else
#define BSTR_CALL(fn, ...) fn##Impl(__VA_ARGS__)
#define BSTR_CALL_NO_TRACKING(fn, ...) fn##Impl(__VA_ARGS__)
#endif

/* Just a length safe wrapper for memmove. */

#define bBlockCopy(D,S,L) { if ((L) > 0) memmove ((D),(S),(L)); }

/* BEGIN The Forge Additional functions */

static inline bool bstrIntersects(const bstring* string, const void* ptr)
{
	return 
		(const unsigned char*)ptr >= string->data &&
		(const unsigned char*)ptr < string->data + bmlen(string);
}

char gEmptyStringBuffer[1] = "";

#ifdef __has_feature
#if __has_feature(thread_sanitizer)
#define HAS_TSAN
#endif
#endif

#ifdef HAS_TSAN
/* Data race here is known to have expected behavior for gEmptyStringBuffer */
__attribute__((no_sanitize("thread")))
static inline void writeNullNoSanitize(bstring* str, int i)
{
	str->data[i] = '\0';
}
static inline void writeNull(bstring* str, int i)
{
	if (str->data != (unsigned char*)gEmptyStringBuffer)
		str->data[i] = '\0';
	else
		writeNullNoSanitize(str, i);
}
#else
#define writeNull(str, i) ((str)->data[(i)] = '\0', (void)0)
#endif


#ifdef AUTOMATED_TESTING
static const bstring gInvalidBstring = { 0,0,NULL };
#endif
#define bisglobalempty(b) ((b)->data == (unsigned char*)&gEmptyStringBuffer[0])

/* END The Forge additional functions */



/* Compute the snapped size for a given requested size.  By snapping to powers
   of 2 like this, repeated reallocations are avoided. */
static int snapUpSize (int i) {
	if (i < 8) {
		i = 8;
	} else {
		unsigned int j;
		j = (unsigned int) i;

		j |= (j >>  1);
		j |= (j >>  2);
		j |= (j >>  4);
		j |= (j >>  8);		/* Ok, since int >= 16 bits */
#if (UINT_MAX != 0xffff)
		j |= (j >> 16);		/* For 32 bit int systems */
#if (UINT_MAX > 0xffffffffUL)
		j |= (j >> 32);		/* For 64 bit int systems */
#endif
#endif
		/* Least power of two greater than i */
		j++;
		if ((int) j >= i) i = (int) j;
	}
	return i;
}


/* Dynamic string initialization functions */
BSTR_DECLARE_FN(bstring, bdynfromcstr, const char* str)
{
	BSTR_TEST_ASSERT_RET(str, gInvalidBstring);
	ASSERT(str);
	bstring out = bempty();
	int ret = bassigncstr(&out, str);
	BSTR_TEST_ASSERT_RET(ret == BSTR_OK, gInvalidBstring);
	ASSERT(ret == BSTR_OK);
	return out;
}

BSTR_DECLARE_FN(bstring, bdynallocfromcstr, const char* str, int minCapacity)
{
	BSTR_TEST_ASSERT_RET(str && minCapacity >= 0, gInvalidBstring);
	ASSERT(str && minCapacity >= 0);
	int slen = (int)strlen(str);
	return bdynfromblk(str, slen, minCapacity);
}

BSTR_DECLARE_FN(bstring, bdynfromblk, const void* blk, int len, int minCapacity)
{
	BSTR_TEST_ASSERT_RET(len >= 0 && minCapacity >= 0 && blk != NULL, gInvalidBstring);
	ASSERT(len >= 0 && minCapacity >= 0 && blk != NULL);
	bstring out = bempty();
	int capacity = len + 1 > minCapacity ? len + 1 : minCapacity;
	int ret = balloc(&out, capacity);
	BSTR_TEST_ASSERT_RET(ret == BSTR_OK, gInvalidBstring);
	ASSERT(ret == BSTR_OK);
	if (len)
		memcpy(out.data, blk, len);
	
	ASSERT(bmlen(&out) > out.slen);
	writeNull(&out, len);
	out.slen = len;
	return out;
}

BSTR_DECLARE_FN(bstring, bdynfromstr, const bstring* b1, int minCapacity)
{
	BSTR_TEST_ASSERT_RET(b1, gInvalidBstring);
	ASSERT(b1);
	return bdynfromblk(b1->data, b1->slen, minCapacity);
}

/*
 * Makes string dynamic if it was static 
 * allocated buffer will be at least len
 * Fails if BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION is not defined
 */
BSTR_DECLARE_FN(int, bmakedynamic, bstring*  b, int len)
{
#ifdef ENABLE_MEMORY_TRACKING
	char fnNameBuf[128];
	bstring fnName = bemptyfromarr(fnNameBuf);
	bformat(&fnName, "%s(propagated from %s)", FUNCTION_NAME, PARENT_FUNCTION_NAME);
	ASSERT(!bownsdata(&fnName));
	const char* pFunction = (const char*)fnName.data;
#else
	#define FILE_NAME __FILE__
	#define FILE_LINE  __LINE__
	#define FUNCTION_NAME __FUNCTION__
	const char* pFunction = FUNCTION_NAME;
#endif
	BSTR_TEST_ASSERT(len >= 0 && bisvalid(b));
	ASSERT(len >= 0 && bisvalid(b));
	bool isDynamic = bownsdata(b);
	if (isDynamic)
		return BSTR_OK;
#ifndef BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION
	BSTR_TEST_ASSERT(bisglobalempty(b));
	ASSERT(bisglobalempty(b));
#endif // !BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION
	len = b->slen + 1 > len ? b->slen + 1 : len;
	int minlen = len;
	len = snapUpSize(minlen);

	unsigned char* x = (unsigned char*)tf_malloc_internal(len, FILE_NAME, FILE_LINE, pFunction);
	/* Try allocating smaller memory */
	if (x == NULL)
		x = (unsigned char*)tf_malloc_internal((len = minlen), FILE_NAME, FILE_LINE, pFunction);

	ASSERT(x != NULL);
	if (b->slen)
		memcpy(x, b->data, b->slen);

	b->data = x;
	ASSERT(len > b->slen);
	writeNull(b, b->slen);
	b->mlen = len | BSTR_DYNAMIC_BIT;
	return BSTR_OK;
}

/*
 * Makes string dynamic if it was static
 * Allocated buffer will be b->slen + 1 or len
 * Fails if BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION is not defined
 */
BSTR_DECLARE_FN(int, bmakedynamicmin, bstring*  b, int len)
{
#ifdef ENABLE_MEMORY_TRACKING
	unsigned char fnNameBuf[128];
	bstring fnName = bemptyfromarr(fnNameBuf);
	bformat(&fnName, "%s(propagated from %s)", FUNCTION_NAME, PARENT_FUNCTION_NAME);
	ASSERT(!bownsdata(&fnName));
	const char* pFunction = (const char*)fnName.data;
#else
	#define FILE_NAME __FILE__
	#define FILE_LINE  __LINE__
	#define FUNCTION_NAME __FUNCTION__
	const char* pFunction = FUNCTION_NAME;
#endif

	BSTR_TEST_ASSERT(len >= 0 && bisvalid(b));
	ASSERT(len >= 0 && bisvalid(b));
	bool is_dynamic = bownsdata(b);
	if (is_dynamic)
		return BSTR_OK;

#ifndef BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION
	/* Empty string can always be converted to dynamic*/
	BSTR_TEST_ASSERT(bisglobalempty(b));
	ASSERT(bisglobalempty(b));
#endif // !BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION
	if (len < b->slen + 1)
		len = b->slen + 1;

	unsigned char* x = (unsigned char*)tf_malloc_internal(len, FILE_NAME, FILE_LINE, pFunction);
	ASSERT(x != NULL);

	if (b->slen)
		memcpy(x, b->data, b->slen);

	b->data = x;
	ASSERT(len > b->slen);
	writeNull(b, b->slen);
	b->mlen = len | BSTR_DYNAMIC_BIT;
	return BSTR_OK;
}

/*  int balloc (bstring* b, int len)
 *
 *  Increase the size of the memory backing the bstring b to at least len.
 *
 *  Doesn't realloc if capacity is sufficient
 */
BSTR_DECLARE_FN(int, balloc, bstring* b, int olen) 
{
#ifdef ENABLE_MEMORY_TRACKING
	char fnNameBuf[128];
	int res = snprintf(fnNameBuf, 128, "%s(propagated from %s)", FUNCTION_NAME, PARENT_FUNCTION_NAME);
	ASSERT(res < 128);
	const char* pFunction = fnNameBuf;
#else
	#define FILE_NAME __FILE__
	#define FILE_LINE  __LINE__
	#define FUNCTION_NAME __FUNCTION__
	const char* pFunction = FUNCTION_NAME;
#endif

	BSTR_TEST_ASSERT(olen >= 0 && bisvalid(b));
	ASSERT(olen >= 0 && bisvalid(b));
	int len;

	int mlen = bmlen(b);

	if (olen > mlen) 
	{
		if (!bownsdata(b))
			return bmakedynamic(b, olen);

		unsigned char * x;

		if ((len = snapUpSize (olen)) <= mlen) return BSTR_OK;

		/* Assume probability of a non-moving realloc is 0.125 */
		if (7 * mlen < 8 * b->slen) 
		{

			/* If slen is close to mlen in size then use realloc to reduce
			   the memory defragmentation */

			reallocStrategy:;

			x = (unsigned char *)tf_realloc_internal(b->data, (size_t)len, FILE_NAME, FILE_LINE, pFunction);
			if (x == NULL) {

				/* Since we failed, try allocating the tighest possible
				   allocation */

				len = olen;
				x = (unsigned char *)tf_realloc_internal(b->data, (size_t) olen, FILE_NAME, FILE_LINE, pFunction);
				ASSERT(x);
			}
		} 
		else 
		{

			/* If slen is not close to mlen then avoid the penalty of copying
			   the extra bytes that are allocated, but not considered part of
			   the string */

			if (NULL == (x = (unsigned char *) tf_malloc_internal((size_t) len, FILE_NAME, FILE_LINE, pFunction)))
			{

				/* Perhaps there is no available memory for the two
				   allocations to be in memory at once */

				goto reallocStrategy;

			} 
			else 
			{
				if (b->slen) 
					memcpy ((char *) x, (char *) b->data, (size_t) b->slen);
				
				tf_free_internal(b->data, FILE_NAME, FILE_LINE, pFunction);
			}
		}
		b->data = x;
		b->mlen = len | BSTR_DYNAMIC_BIT;
		ASSERT(len > b->slen);
		writeNull(b, b->slen);

#if defined (BSTRLIB_TEST_CANARY)
		if (len > b->slen + 1)
			memchr (b->data + b->slen + 1, 'X', len - (b->slen + 1));
#endif
	}

	return BSTR_OK;
}

/*  int ballocmin (bstring b, int len)
 *
 *  Set the size of the memory backing the bstring b to len or b->slen+1,
 *  whichever is larger.  Note that repeated use of this function can degrade
 *  performance.
 * 
 *  TF: If the bstring is not dynamic tries to convert to dynamic 
 *      only if capacity is not sufficient
 */
BSTR_DECLARE_FN(int, ballocmin, bstring*  b, int len) {
#ifdef ENABLE_MEMORY_TRACKING
	unsigned char fnNameBuf[128];
	bstring fnName = bemptyfromarr(fnNameBuf);
	bformat(&fnName, "%s(propagated from %s)", FUNCTION_NAME, PARENT_FUNCTION_NAME);
	ASSERT(!bownsdata(&fnName));
	const char* pFunction = (const char*)fnName.data;
#else
	#define FILE_NAME __FILE__
	#define FILE_LINE  __LINE__
	#define FUNCTION_NAME __FUNCTION__
	const char* pFunction = FUNCTION_NAME;
#endif

	unsigned char * s;

	BSTR_TEST_ASSERT(len >= 0 && bisvalid(b));
	ASSERT(len >= 0 && bisvalid(b));

	int mlen = bmlen(b);

	if (len < b->slen + 1)
		len = b->slen + 1;

	if (!bownsdata(b))
	{
		if (mlen < len)
			return bmakedynamicmin(b, len);
		return BSTR_OK;
	}

	if (len != mlen) 
	{
		s = (unsigned char *) tf_realloc_internal(b->data, (size_t) len, FILE_NAME, FILE_LINE, pFunction);
		ASSERT(s != NULL);
		ASSERT(len > b->slen);
		b->data = s;
		writeNull(b, b->slen);
		b->mlen = len | BSTR_DYNAMIC_BIT;
	}

	return BSTR_OK;
}


/*  int bconcat (bstring* b0, const bstring* b1)
 *
 *  Concatenate the bstring b1 to the bstring b0.
 *
 *  TF: Now calls bcatblk
 */
BSTR_DECLARE_FN(int, bconcat, bstring* b0, const bstring* b1) {
	BSTR_TEST_ASSERT(b1);
	ASSERT(b1);
	return bcatblk(b0, b1->data, b1->slen);
}

/*  int bconchar (bstring* b, char c)
 *
 *  Concatenate the single character c to the bstring b.
 */
BSTR_DECLARE_FN(int, bconchar, bstring* b, char c) {

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	int d = b->slen;

	int allocResult = balloc(b, d + 2);
	ASSERT(allocResult == BSTR_OK);

	b->data[d] = (unsigned char)c;
	ASSERT(bmlen(b) > (d + 1));
	writeNull(b, d+1);
	b->slen++;
	return BSTR_OK;
}

/*  int bcatcstr (bstring* b, const char * s)
 *
 *  Concatenate a char * string to a bstring.
 */
BSTR_DECLARE_FN(int, bcatcstr, bstring*  b, const char * s) {
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(s);
	ASSERT(s);
	/* 
	 * This function doesn't handle aliasing memory
	 */
	BSTR_TEST_ASSERT(!bstrIntersects(b, s));
	ASSERT(!bstrIntersects(b, s));

	char * d;
	int i, l;

	/* Optimistically concatenate directly */
	int mlen = bmlen(b);

	/* -1 is done so that null terminator is not modified in case of an error */
	l = mlen - b->slen - 1;
	d = (char *) &b->data[b->slen];
	for (i=0; i < l; i++) {
		if ((*d++ = *s++) == '\0') {
			b->slen += i;
			return BSTR_OK;
		}
	}
	b->slen += i;

	/* Need to explicitely resize and concatenate tail */
	return bcatblk (b, (const void *) s, (int) strlen (s));
}

/*  int bcatblk (bstring* b, const void * s, int len)
 *
 *  Concatenate a fixed length buffer to a bstring.
 *
 *  TF:
 *  + Handles aliasing
 *  + Tries to use stack allocated buffers
 */
BSTR_DECLARE_FN(int, bcatblk, bstring* b, const void* s, int len) {

	int d, err, mlen, requiredCapacity;

	BSTR_TEST_ASSERT(b && bisvalid(b));
	ASSERT(b && bisvalid(b));
	BSTR_TEST_ASSERT(len >= 0 && s != NULL);
	ASSERT(len >= 0 && s != NULL);

	bstring aux;
	aux.data = (unsigned char*)s;
	aux.mlen = 0;
	aux.slen = len;

	/*
	 * The Forge: when conversion is allowed try
	 * storing temporary on the stack when aliasing occurs
	 */
	unsigned char auxBuffer[BSTR_TMP_BUFFER_SIZE] = "";

	d = b->slen;

	/* Overflow */
	BSTR_TEST_ASSERT(len < INT_MAX - 1 && d < INT_MAX - len - 1);
	ASSERT(len < INT_MAX - 1 && d < INT_MAX - len - 1);

	requiredCapacity = d + len + 1;
	mlen = bmlen(b);


	if (mlen < requiredCapacity) {
		ptrdiff_t pd = (unsigned char*)s - b->data;
		/* aliasing */
		if (pd >= 0 && pd < mlen) {
			
			if (len + 1 <= BSTR_TMP_BUFFER_SIZE)
			{
				aux.data = (unsigned char*)auxBuffer;
				aux.mlen = BSTR_TMP_BUFFER_SIZE;
				aux.slen = 0;
			}
			else
			{
				aux = bempty();
			}
			
			err = bassignblk(&aux, s, len);
			ASSERT(err == BSTR_OK);
		}
		err = balloc(b, requiredCapacity);
		ASSERT(err == BSTR_OK);
	}

	memmove(&b->data[d], aux.data, (size_t)len);
	ASSERT(bmlen(b) > (d + len));
	writeNull(b, d + len);
	b->slen = d + len;
	if (aux.data != s) 
		bdestroy(&aux);
	return BSTR_OK;
}

BSTR_DECLARE_FN(int, bmakecstr, bstring*  b)
{
	int ret = BSTR_OK;
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	ret = balloc(b, b->slen + 1);
	ASSERT(ret == BSTR_OK);
	ASSERT(bmlen(b) > b->slen);
	writeNull(b, b->slen);
	return ret;
}

/*  int bassign (bstring* a, const bstring* b)
 *
 *  Overwrite the string a with the contents of string b.
 *  
 *  TF: Calls bassignblk
 */
BSTR_DECLARE_FN(int, bassign, bstring* a, const bstring* b) {
	BSTR_TEST_ASSERT(b);
	ASSERT(b);
	return bassignblk(a, b->data, b->slen);
}

/*  int bassignmidstr (bstring* a, const bstring* b, int left, int len)
 *
 *  Overwrite the string a with the middle of contents of string b
 *  starting from position left and running for a length len.
 *
 *  TF: Calls bassignblk
 */
BSTR_DECLARE_FN(int, bassignmidstr, bstring* a, const bstring* b, int left, int len) {
	
	BSTR_TEST_ASSERT(bconstisvalid(b));
	ASSERT(bconstisvalid(b));
	BSTR_TEST_ASSERT(left >= 0 && left <= b->slen && len >= 0);
	ASSERT(left >= 0 && left <= b->slen && len >= 0);

	if (len > b->slen - left) 
		len = b->slen - left;

	return bassignblk(a, b->data + left, len);
}

/*  int bassigncstr (bstring* a, const char * str)
 *
 *  Overwrite the string a with the contents of char * string str.  Note that
 *  the bstring a must be a well defined and writable bstring.
 */
BSTR_DECLARE_FN(int, bassigncstr, bstring* a, const char* str) {
	int i;
	size_t len;
	BSTR_TEST_ASSERT(bisvalid(a));
	ASSERT(bisvalid(a));
	BSTR_TEST_ASSERT(str);
	ASSERT(str);
	BSTR_TEST_ASSERT(!bstrIntersects(a, str));
	ASSERT(!bstrIntersects(a, str));

	/* -1 is done so that null terminator is not modified(for empty strings) */
	int mlen = bmlen(a) - 1;

	for (i = 0; i < mlen && str[i]; i++) {
		a->data[i] = str[i];
	}

	len = strlen(str + i);
	a->slen = i;
	/* Ensure null terminator is still written even if alloc below fails */
	ASSERT(bmlen(a) > i);
	writeNull(a, i);

	if (len != 0)
	{
	BSTR_TEST_ASSERT((size_t)INT_MAX - i > len + 1);
	ASSERT((size_t)INT_MAX - i > len + 1);
		int allocResult = balloc(a, (int)(i + len + 1));
		ASSERT(allocResult == BSTR_OK);
		memcpy(a->data + i, str + i, (size_t)len + 1);
		a->slen += (int)len;
	}

	return BSTR_OK;
}

/*  int bassignblk (bstring a, const void * s, int len)
 *
 *  Overwrite the string a with the contents of the block (s, len).  Note that
 *  the bstring a must be a well defined and writable bstring.
 * 
 *  TF: Handles special aliasing case:
 *      a->data == s && len == a->mlen
 *      This case was deallocating and using s previously
 */
BSTR_DECLARE_FN(int, bassignblk, bstring* a, const void* s, int len) {

	BSTR_TEST_ASSERT(bisvalid(a));
	ASSERT(bisvalid(a));
	BSTR_TEST_ASSERT(len >= 0 && s != NULL);
	ASSERT(len >= 0 && s != NULL);

	if (len != 0)
	{
		ptrdiff_t pd = (unsigned char*)s - a->data;

		int allocResult = balloc(a, len + 1);
		ASSERT(allocResult == BSTR_OK);

		if (pd >= 0 && pd < bmlen(a))
		{
			BSTR_TEST_ASSERT(pd + len <= a->slen );
			ASSERT(pd + len <= a->slen && "balloc might've erased s content.");
			s = a->data + pd;
		}

		memmove(a->data, s, len);
	}

	ASSERT(bmlen(a) > len);
	writeNull(a, len);
	a->slen = len;
	return BSTR_OK;
}


/*  int btrunc (bstring b, int n)
 *
 *  Truncate the bstring to at most n characters.
 */
BSTR_DECLARE_FN(int, btrunc, bstring*  b, int n) {
    #ifdef ENABLE_MEMORY_TRACKING
	UNREF_PARAM(PARENT_FUNCTION_NAME);
	UNREF_PARAM(FUNCTION_NAME);
	UNREF_PARAM(FILE_NAME);
	UNREF_PARAM(FILE_LINE);
    #endif
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(n >= 0);
	ASSERT(n >= 0);

	if (b->slen > n) {
		b->slen = n;
		ASSERT(bmlen(b) > n);
		writeNull(b, n);
	}
	return BSTR_OK;
}

#define   upcase(c) (toupper ((unsigned char) c))
#define downcase(c) (tolower ((unsigned char) c))
#define   wspace(c) (isspace ((unsigned char) c))

/*  int btoupper (bstring b)
 *
 *  Convert contents of bstring to upper case.
 */
int btoupper (bstring* b) {
	int i, len;
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	for (i=0, len = b->slen; i < len; ++i) {
		b->data[i] = (unsigned char) upcase (b->data[i]);
	}
	return BSTR_OK;
}

/*  int btolower (bstring b)
 *
 *  Convert contents of bstring to lower case.
 */
int btolower (bstring* b) {
	int i, len;
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	for (i=0, len = b->slen; i < len; i++) {
		b->data[i] = (unsigned char) downcase (b->data[i]);
	}
	return BSTR_OK;
}

/*  int bstricmp (const bstring* b0, const bstring* b1)
 *
 *  Compare two strings without differentiating between case. The return
 *  value is the difference of the values of the characters where the two
 *  strings first differ after lower case transformation, otherwise 0 is
 *  returned indicating that the strings are equal. If the lengths are
 *  different, then a difference from 0 is given, but if the first extra
 *  character is '\0', then it is taken to be the value BSTR_CMP_EXTRA_NULL.
 */
int bstricmp (const bstring* b0, const bstring* b1) {
	int i, v, n;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));

	if ((n = b0->slen) > b1->slen) 
		n = b1->slen;
	else if (b0->slen == b1->slen && b0->data == b1->data)
		return BSTR_OK;

	for (i = 0; i < n; i ++) {
		v  = (char) downcase (b0->data[i])
		   - (char) downcase (b1->data[i]);
		if (0 != v) 
			return v;
	}

	if (b0->slen > n) {
		v = (char) downcase (b0->data[n]);
		if (v) 
			return v;
		return BSTR_CMP_EXTRA_NULL;
	}
	if (b1->slen > n) {
		v = - (char) downcase (b1->data[n]);
		if (v) 
			return v;
		return -BSTR_CMP_EXTRA_NULL;
	}
	return BSTR_OK;
}

/*  int bstrnicmp (const bstring* b0, const bstring* b1, int n)
 *
 *  Compare two strings without differentiating between case for at most n
 *  characters.  If the position where the two strings first differ is
 *  before the nth position, the return value is the difference of the values
 *  of the characters, otherwise 0 is returned.  If the lengths are different
 *  and less than n characters, then a difference from 0 is given, but if the
 *  first extra character is '\0', then it is taken to be the value
 *  BSTR_CMP_EXTRA_NULL.
 */
int bstrnicmp (const bstring* b0, const bstring* b1, int n) {
	int i, v, m;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));

	m = n;
	if (m > b0->slen) 
		m = b0->slen;
	if (m > b1->slen) 
		m = b1->slen;

	if (b0->data != b1->data) {
		for (i = 0; i < m; i ++) {
			v  = (char) downcase (b0->data[i]);
			v -= (char) downcase (b1->data[i]);
			if (v != 0) 
				return b0->data[i] - b1->data[i];
		}
	}

	if (n == m || b0->slen == b1->slen) 
		return BSTR_OK;

	if (b0->slen > m) {
		v = (char) downcase (b0->data[m]);
		if (v) 
			return v;
		return BSTR_CMP_EXTRA_NULL;
	}

	v = - (char) downcase (b1->data[m]);
	if (v) 
		return v;
	return -BSTR_CMP_EXTRA_NULL;
}

/*  int biseqcaselessblk (const bstring* b, const void * blk, int len)
 *
 *  Compare content of b and the array of bytes in blk for length len for
 *  equality without differentiating between character case.  If the content
 *  differs other than in case, 0 is returned, if, ignoring case, the content
 *  is the same, 1 is returned, if there is an error, -1 is returned.  If the
 *  length of the strings are different, this function is O(1).  '\0'
 *  characters are not treated in any special way.
 */
int biseqcaselessblk (const bstring* b, const void * blk, int len) {
	int i;
	BSTR_TEST_ASSERT(bconstisvalid(b));
	ASSERT(bconstisvalid(b));
	BSTR_TEST_ASSERT(len >= 0 && blk != NULL);
	ASSERT(len >= 0 && blk != NULL);

	if (b->slen != len) 
		return 0;
	if (len == 0 || b->data == blk) 
		return 1;
	for (i=0; i < len; i++) {
		if (b->data[i] != ((unsigned char*)blk)[i]) {
			unsigned char c = (unsigned char) downcase (b->data[i]);
			if (c != (unsigned char) downcase (((unsigned char*)blk)[i]))
				return 0;
		}
	}
	return 1;
}


/*  int biseqcaseless (const bstring* b0, const bstring* b1)
 *
 *  Compare two strings for equality without differentiating between case.
 *  If the strings differ other than in case, 0 is returned, if the strings
 *  are the same, 1 is returned, if there is an error, -1 is returned.  If
 *  the length of the strings are different, this function is O(1).  '\0'
 *  termination characters are not treated in any special way.
 */
int biseqcaseless (const bstring* b0, const bstring* b1) {
	BSTR_TEST_ASSERT(b1 != NULL);
	ASSERT(b1 != NULL);
	return biseqcaselessblk (b0, b1->data, b1->slen);
}

/*  int bisstemeqcaselessblk (const bstring* b0, const void * blk, int len)
 *
 *  Compare beginning of string b0 with a block of memory of length len
 *  without differentiating between case for equality.  If the beginning of b0
 *  differs from the memory block other than in case (or if b0 is too short),
 *  0 is returned, if the strings are the same, 1 is returned, if there is an
 *  error, -1 is returned.  '\0' characters are not treated in any special
 *  way.
 */
int bisstemeqcaselessblk (const bstring* b0, const void * blk, int len) {
	int i;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(len >= 0 && blk != NULL);
	ASSERT(len >= 0 && blk != NULL);

	if (b0->slen < len) return BSTR_OK;
	if (b0->data == (const unsigned char *) blk || len == 0) return 1;

	for (i = 0; i < len; i ++) {
		if (b0->data[i] != ((const unsigned char *) blk)[i]) {
			if (downcase (b0->data[i]) !=
			    downcase (((const unsigned char *) blk)[i])) return 0;
		}
	}
	return 1;
}

/*
 * int bltrimws (bstring* b)
 *
 * Delete whitespace contiguous from the left end of the string.
 */
int bltrimws (bstring* b) {
	int i, len, ret;
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	for (len = b->slen, i = 0; i < len; i++) {
		if (!wspace (b->data[i])) {
			ret = bdelete (b, 0, i);
			if (bmlen(b) > b->slen)
				writeNull(b, b->slen);
			return ret;
		}
	}

	writeNull(b, 0);
	b->slen = 0;
	return BSTR_OK;
}

/*
 * int brtrimws (bstring* b)
 *
 * Delete whitespace contiguous from the right end of the string.
 */
int brtrimws (bstring* b) {
	int i;

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	int mlen = bmlen(b);

	for (i = b->slen - 1; i >= 0; i--) {
		if (!wspace (b->data[i])) {
			if (mlen > i + 1)
				writeNull(b, i + 1);
			b->slen = i + 1;
			return BSTR_OK;
		}
	}

	writeNull(b, 0);
	b->slen = 0;
	return BSTR_OK;
}

/*
 * int btrimws (bstring b)
 *
 * Delete whitespace contiguous from both ends of the string.
 */
int btrimws (bstring* b) {
	int i, j;

	BSTR_TEST_ASSERT(b && bisvalid(b));
	ASSERT(b && bisvalid(b));

	int mlen = bmlen(b);

	for (i = b->slen - 1; i >= 0; i--) {
		if (!wspace (b->data[i])) {
			if (mlen > i + 1)
				writeNull(b, i + 1);
			b->slen = i + 1;
			for (j = 0; wspace (b->data[j]); j++) {}
			return bdelete (b, 0, j);
		}
	}
	
	writeNull(b, 0);
	b->slen = 0;
	return BSTR_OK;
}

/*  int biseqblk (const bstring* b, const void * blk, int len)
 *
 *  Compare the string b with the character block blk of length len.  If the
 *  content differs, 0 is returned, if the content is the same, 1 is returned.  
 *  If the length of the strings are different, this function is O(1). 
 *  '\0' characters are not treated in any special way.
 */
int biseqblk (const bstring* b, const void * blk, int len) {
	BSTR_TEST_ASSERT(bconstisvalid(b));
	ASSERT(bconstisvalid(b));
	BSTR_TEST_ASSERT(len >= 0 && blk != NULL);
	ASSERT(len >= 0 && blk != NULL);

	if (b->slen != len) return 0;
	if (len == 0 || b->data == blk) return 1;
	return !memcmp (b->data, blk, len);
}

/*  int biseq (const bstring* b0, const bstring* b1)
 *
 *  Compare the string b0 and b1.  If the strings differ, 0 is returned, if
 *  the strings are the same, 1 is returned, if there is an error, -1 is
 *  returned.  If the length of the strings are different, this function is
 *  O(1).  '\0' termination characters are not treated in any special way.
 */
int biseq (const bstring* b0, const bstring* b1) {
	BSTR_TEST_ASSERT(b1);
	ASSERT(b1);
	return biseqblk(b0, b1->data, b1->slen);
}

/*  int bisstemeqblk (const bstring* b0, const void * blk, int len)
 *
 *  Compare beginning of string b0 with a block of memory of length len for
 *  equality.  If the beginning of b0 differs from the memory block (or if b0
 *  is too short), 0 is returned, if the strings are the same, 1 is returned. 
 *  '\0' characters are not treated in any special way.
 */
int bisstemeqblk (const bstring* b0, const void * blk, int len) {
	int i;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(len >= 0 && blk != NULL);
	ASSERT(len >= 0 && blk != NULL);

	if (b0->slen < len) return BSTR_OK;
	if (b0->data == (const unsigned char *) blk || len == 0) return 1;

	for (i = 0; i < len; i ++) {
		if (b0->data[i] != ((const unsigned char *) blk)[i]) return BSTR_OK;
	}
	return 1;
}

/*  int biseqcstr (const bstring* b, const char *s)
 *
 *  Compare the bstring b and char * string s.  The C string s must be '\0'
 *  terminated at exactly the length of the bstring b, and the contents
 *  between the two must be identical with the bstring b with no '\0'
 *  characters for the two contents to be considered equal.  This is
 *  equivalent to the condition that their current contents will be always be
 *  equal when comparing them in the same format after converting one or the
 *  other.  If the strings are equal 1 is returned, if they are unequal 0 is
 *  returned.
 */
int biseqcstr (const bstring* b, const char * s) {
	int i;

	BSTR_TEST_ASSERT(bconstisvalid(b));
	ASSERT(bconstisvalid(b));
	BSTR_TEST_ASSERT(s);
	ASSERT(s);

	for (i=0; i < b->slen; i++) {
		if (s[i] == '\0' || b->data[i] != (unsigned char) s[i])
			return BSTR_OK;
	}
	return s[i] == '\0';
}

/*  int biseqcstrcaseless (const bstring* b, const char *s)
 *
 *  Compare the bstring b and char * string s.  The C string s must be '\0'
 *  terminated at exactly the length of the bstring b, and the contents
 *  between the two must be identical except for case with the bstring b with
 *  no '\0' characters for the two contents to be considered equal.  This is
 *  equivalent to the condition that their current contents will be always be
 *  equal ignoring case when comparing them in the same format after
 *  converting one or the other.  If the strings are equal, except for case,
 *  1 is returned, if they are unequal regardless of case 0 is returned.
 */
int biseqcstrcaseless (const bstring* b, const char * s) {
	
	int i;

	BSTR_TEST_ASSERT(bconstisvalid(b));
	ASSERT(bconstisvalid(b));
	BSTR_TEST_ASSERT(s);
	ASSERT(s);

	for (i=0; i < b->slen; i++) {
		if (s[i] == '\0' ||
		    (b->data[i] != (unsigned char) s[i] &&
		     downcase (b->data[i]) != (unsigned char) downcase (s[i])))
			return BSTR_OK;
	}
	return s[i] == '\0';
}

/*  int bstrcmp (const bstring* b0, const bstring* b1)
 *
 *  Compare the string b0 and b1. A value less than or greater than zero, indicating that the
 *  string pointed to by b0 is lexicographically less than or greater than
 *  the string pointed to by b1 is returned.  If the the string lengths are
 *  unequal but the characters up until the length of the shorter are equal
 *  then a value less than, or greater than zero, indicating that the string
 *  pointed to by b0 is shorter or longer than the string pointed to by b1 is
 *  returned.  0 is returned if and only if the two strings are the same.  If
 *  the length of the strings are different, this function is O(n).  Like its
 *  standard C library counter part strcmp, the comparison does not proceed
 *  past any '\0' termination characters encountered.
 */
int bstrcmp (const bstring* b0, const bstring* b1) {
	int i, v, n;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));

	n = b0->slen; 
	if (n > b1->slen) 
		n = b1->slen;
	if (b0->slen == b1->slen && (b0->data == b1->data || b0->slen == 0))
		return BSTR_OK;

	for (i = 0; i < n; i ++) {
		v = ((char) b0->data[i]) - ((char) b1->data[i]);
		if (v != 0) return v;
		if (b0->data[i] == (unsigned char) '\0') return BSTR_OK;
	}

	if (b0->slen > n) return 1;
	if (b1->slen > n) return -1;
	return BSTR_OK;
}

/*  int bstrncmp (const bstring* b0, const bstring* b1, int n)
 *
 *  Compare the string b0 and b1 for at most n characters.  A value is returned 
 *  as if b0 and b1 were first truncated to at most n characters then bstrcmp 
 *  was called with these new strings are paremeters.  If the length of the strings
 *  are different, this function is O(n).  Like its standard C library counter
 *  part strcmp, the comparison does not proceed past any '\0' termination
 *  characters encountered.
 */
int bstrncmp (const bstring* b0, const bstring* b1, int n) {
	int i, v, m;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));

	m = n;
	if (m > b0->slen) 
		m = b0->slen;
	if (m > b1->slen) 
		m = b1->slen;

	if (b0->data != b1->data) {
		for (i = 0; i < m; i ++) {
			v = ((char) b0->data[i]) - ((char) b1->data[i]);
			if (v != 0) 
				return v;
			if (b0->data[i] == (unsigned char) '\0') 
				return BSTR_OK;
		}
	}

	if (n == m || b0->slen == b1->slen) 
		return BSTR_OK;

	if (b0->slen > m) return 1;
	return -1;
}

/*  int bdelete (bstring b, int pos, int len)
 *
 *  Removes characters from pos to pos+len-1 inclusive and shifts the tail of
 *  the bstring starting from pos+len to pos.  len must be positive for this
 *  call to have any effect.
 */
int bdelete(bstring* b, int pos, int len) 
{
	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(pos >= 0 && pos <= b->slen && len >= 0);
	ASSERT(pos >= 0 && pos <= b->slen && len >= 0);

	if (len > 0 && pos < b->slen) {
		if ((INT_MAX - len) <= pos || pos + len >= b->slen) {
			b->slen = pos;
		} else {
			bBlockCopy ((char *) (b->data + pos),
			            (char *) (b->data + pos + len),
			            b->slen - (pos+len));
			b->slen -= len;
		}
		ASSERT(bmlen(b) > b->slen);
		writeNull(b, b->slen);
	}
	return BSTR_OK;
}

/*  int bdestroy (bstring b)
 *
 *  Free up the bstring.  Note that if b is detectably invalid or not writable
 *  then no action is performed and BSTR_ERR is returned.  Like a freed memory
 *  allocation, dereferences, writes or any other action on b after it has
 *  been bdestroyed is undefined.
 */
BSTR_DECLARE_FN(int, bdestroy, bstring* b) {

#ifdef ENABLE_MEMORY_TRACKING
	unsigned char fnNameBuf[128];
	bstring fnName = bemptyfromarr(fnNameBuf);
	bformat(&fnName, "%s(propagated from %s)", FUNCTION_NAME, PARENT_FUNCTION_NAME);
	ASSERT(!bownsdata(&fnName));
	const char* pFunction = (const char*)fnName.data;
#else
	#define FILE_NAME __FILE__
	#define FILE_LINE  __LINE__
	#define FUNCTION_NAME __FUNCTION__
	const char* pFunction = FUNCTION_NAME;
#endif

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));

	if (bownsdata(b))
		tf_free_internal(b->data, FILE_NAME, FILE_LINE, pFunction);

#ifdef BSTR_ALLOW_USAGE_AFTER_DESTROY
	*b = bempty();
#else
	b->slen = -1;
	b->mlen = 0;
	b->data = NULL;
#endif // BSTR_ALLOW_USAGE_AFTER_DESTROY

	/* TF: We never own tagbstring
	tf_free (b);
	*/
	return BSTR_OK;
}

/*  int binstr (const bstring* b1, int pos, const bstring* b2)
 *
 *  Search for the bstring b2 in b1 starting from position pos, and searching
 *  forward.  If it is found then return with the first position where it is
 *  found, otherwise return BSTR_ERR.  Note that this is just a brute force
 *  string searcher that does not attempt clever things like the Boyer-Moore
 *  search algorithm.  Because of this there are many degenerate cases where
 *  this can take much longer than it needs to.
 */
int binstr (const bstring* b1, int pos, const bstring* b2) {
	
	int j, ii, ll, lf;
	unsigned char * d0;
	unsigned char c0;
	register unsigned char * d1;
	register unsigned char c1;
	register int i;

	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(bconstisvalid(b2));
	ASSERT(bconstisvalid(b2));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (pos > b1->slen)
		return BSTR_ERR;
	if (b2->slen == 0)
		return pos;
	if (pos == b1->slen)
		return BSTR_ERR;

	/* No space to find such a string? */
	if ((lf = b1->slen - b2->slen + 1) <= pos) 
		return BSTR_ERR;

	/* An obvious alias case */
	if (b1->data == b2->data && pos == 0) 
		return 0;

	i = pos;

	d0 = b2->data;
	d1 = b1->data;
	ll = b2->slen;

	/* Peel off the b2->slen == 1 case */
	c0 = d0[0];
	if (1 == ll) {
		for (;i < lf; i++) 
			if (c0 == d1[i]) 
				return i;
		return BSTR_ERR;
	}

	c1 = c0;
	j = 0;
	lf = b1->slen - 1;

	ii = -1;
	if (i < lf) do {
		/* Unrolled current character test */
		if (c1 != d1[i]) {
			if (c1 != d1[1+i]) {
				i += 2;
				continue;
			}
			i++;
		}

		/* Take note if this is the start of a potential match */
		if (0 == j) 
			ii = i;

		/* Shift the test character down by one */
		j++;
		i++;

		/* If this isn't past the last character continue */
		if (j < ll) {
			c1 = d0[j];
			continue;
		}

		N0:;

		/* If no characters mismatched, then we matched */
		if (i == ii+j) 
			return ii;

		/* Shift back to the beginning */
		i -= j;
		j  = 0;
		c1 = c0;
	} while (i < lf);

	/* Deal with last case if unrolling caused a misalignment */
	if (i == lf && ll == j+1 && c1 == d1[i]) 
		goto N0;

	return BSTR_ERR;
}

/*  int binstrr (const bstring* b1, int pos, const bstring* b2)
 *
 *  Search for the bstring b2 in b1 starting from position pos, and searching
 *  backward.  If it is found then return with the first position where it is
 *  found, otherwise return BSTR_ERR.  Note that this is just a brute force
 *  string searcher that does not attempt clever things like the Boyer-Moore
 *  search algorithm.  Because of this there are many degenerate cases where
 *  this can take much longer than it needs to.
 */
int binstrr (const bstring* b1, int pos, const bstring* b2) {
	int j, i, l;
	unsigned char * d0, * d1;

	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(bconstisvalid(b2));
	ASSERT(bconstisvalid(b2));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (pos > b1->slen)
		pos = b1->slen;

	if (b2->slen == 0)
		return pos;

	/* Obvious alias case */
	if (b1->data == b2->data && pos == 0 && b2->slen <= b1->slen) 
		return 0;

	i = pos;
	if ((l = b1->slen - b2->slen) < 0) 
		return BSTR_ERR;

	/* If no space to find such a string then snap back */
	if (l + 1 <= i) 
		i = l;
	j = 0;

	d0 = b2->data;
	d1 = b1->data;
	l  = b2->slen;

	for (;;) {
		if (d0[j] == d1[i + j]) {
			j ++;
			if (j >= l) 
				return i;
		} else {
			i --;
			if (i < 0) 
				break;
			j=0;
		}
	}

	return BSTR_ERR;
}

/*  int binstrcaseless (const bstring* b1, int pos, const bstring* b2)
 *
 *  Search for the bstring b2 in b1 starting from position pos, and searching
 *  forward but without regard to case.  If it is found then return with the
 *  first position where it is found, otherwise return BSTR_ERR.  Note that
 *  this is just a brute force string searcher that does not attempt clever
 *  things like the Boyer-Moore search algorithm.  Because of this there are
 *  many degenerate cases where this can take much longer than it needs to.
 */
int binstrcaseless (const bstring* b1, int pos, const bstring* b2) {
	int j, i, l, ll;
	unsigned char * d0, * d1;

	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(bconstisvalid(b2));
	ASSERT(bconstisvalid(b2));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (pos > b1->slen)
		return BSTR_ERR;
	if (b2->slen == 0)
		return pos;
	if (pos == b1->slen)
		return BSTR_ERR;


	l = b1->slen - b2->slen + 1;

	/* No space to find such a string? */
	if (l <= pos) 
		return BSTR_ERR;

	/* An obvious alias case */
	if (b1->data == b2->data && pos == 0) 
		return BSTR_OK;

	i = pos;
	j = 0;

	d0 = b2->data;
	d1 = b1->data;
	ll = b2->slen;

	for (;;) {
		if (d0[j] == d1[i + j] || downcase (d0[j]) == downcase (d1[i + j])) {
			j ++;
			if (j >= ll) 
				return i;
		} else {
			i ++;
			if (i >= l) 
				break;
			j=0;
		}
	}

	return BSTR_ERR;
}

/*  int binstrrcaseless (const bstring* b1, int pos, const bstring* b2)
 *
 *  Search for the bstring b2 in b1 starting from position pos, and searching
 *  backward but without regard to case.  If it is found then return with the
 *  first position where it is found, otherwise return BSTR_ERR.  Note that
 *  this is just a brute force string searcher that does not attempt clever
 *  things like the Boyer-Moore search algorithm.  Because of this there are
 *  many degenerate cases where this can take much longer than it needs to.
 */
int binstrrcaseless (const bstring* b1, int pos, const bstring* b2) {
	int j, i, l;
	unsigned char * d0, * d1;

	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(bconstisvalid(b2));
	ASSERT(bconstisvalid(b2));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (pos > b1->slen)
		pos = b1->slen;

	if (b2->slen == 0)
		return pos;

	/* Obvious alias case */
	if (b1->data == b2->data && pos == 0 && b2->slen <= b1->slen)
		return BSTR_OK;

	i = pos;
	if ((l = b1->slen - b2->slen) < 0) 
		return BSTR_ERR;

	/* If no space to find such a string then snap back */
	if (l + 1 <= i) 
		i = l;
	j = 0;

	d0 = b2->data;
	d1 = b1->data;
	l  = b2->slen;

	for (;;) {
		if (d0[j] == d1[i + j] || downcase (d0[j]) == downcase (d1[i + j])) {
			j ++;
			if (j >= l) 
				return i;
		} else {
			i --;
			if (i < 0) 
				break;
			j=0;
		}
	}

	return BSTR_ERR;
}


/*  int bstrchrp (const bstring* b, int c, int pos)
 *
 *  Search for the character c in b forwards from the position pos
 *  (inclusive).
 */
int bstrchrp (const bstring* b, int c, int pos) {
	unsigned char * p;

	BSTR_TEST_ASSERT(bconstisvalid(b));
	ASSERT(bconstisvalid(b));
	BSTR_TEST_ASSERT(pos >= 0 && pos <= b->slen);
	ASSERT(pos >= 0 && pos <= b->slen);

	if (b->slen <= pos)
		return BSTR_ERR;
	p = (unsigned char *) memchr ((b->data + pos), (unsigned char) c,
		                                (b->slen - pos));
	if (p) 
		return (int) (p - b->data);
	return BSTR_ERR;
}

/*  int bstrrchrp (const bstring* b, int c, int pos)
 *
 *  Search for the character c in b backwards from the position pos in string
 *  (inclusive).
 *  
 *  TF: clamps slen to slen - 1, so that bstrrchr can be used with 0 size strings
 */
int bstrrchrp (const bstring* b, int c, int pos) {
	int i;

	BSTR_TEST_ASSERT(b && bconstisvalid(b));
	ASSERT(b && bconstisvalid(b));
	BSTR_TEST_ASSERT(pos >= 0 && pos <= b->slen);
	ASSERT(pos >= 0 && pos <= b->slen);

	pos = pos >= b->slen ? b->slen - 1 : pos;

	for (i=pos; i >= 0; i--) {
		if (b->data[i] == (unsigned char) c) return i;
	}
	return BSTR_ERR;
}

#if !defined (BSTRLIB_AGGRESSIVE_MEMORY_FOR_SPEED_TRADEOFF)
#define LONG_LOG_BITS_QTY (3)
#define LONG_BITS_QTY (1 << LONG_LOG_BITS_QTY)
#define LONG_TYPE unsigned char

#define CFCLEN ((1 << CHAR_BIT) / LONG_BITS_QTY)
struct charField { LONG_TYPE content[CFCLEN]; };
#define testInCharField(cf,c) ((cf)->content[(c) >> LONG_LOG_BITS_QTY] & \
	                           (((long)1) << ((c) & (LONG_BITS_QTY-1))))
#define setInCharField(cf,idx) { \
	unsigned int c = (unsigned int) (idx); \
	(cf)->content[c >> LONG_LOG_BITS_QTY] |= \
		(LONG_TYPE) (1ul << (c & (LONG_BITS_QTY-1))); \
}

#else

#define CFCLEN (1 << CHAR_BIT)
struct charField { unsigned char content[CFCLEN]; };
#define testInCharField(cf,c) ((cf)->content[(unsigned char) (c)])
#define setInCharField(cf,idx) (cf)->content[(unsigned int) (idx)] = (unsigned char)~0

#endif

/* Convert a bstring to charField */
static inline void buildCharField (struct charField * cf, const bstring* b) {
	int i;

	ASSERT(cf && bconstisvalid(b) && b->slen > 0 &&
	       "Calling function should validate inputs");
	memset ((void *) cf->content, 0, sizeof (struct charField));
	for (i=0; i < b->slen; i++) {
		setInCharField(cf, b->data[i]);
	}
}

static inline void invertCharField (struct charField * cf) {
	int i;
	for (i=0; i < CFCLEN; i++) cf->content[i] = ~cf->content[i];
}

/* Inner engine for binchr */
static inline int binchrCF (const unsigned char * data, int len, int pos,
					 const struct charField * cf) {
	int i;
	for (i=pos; i < len; i++) {
		unsigned char c = (unsigned char) data[i];
		if (testInCharField (cf, c)) 
			return i;
	}
	return BSTR_ERR;
}

/*  int binchr (const bstring* b0, int pos, const bstring* b1);
 *
 *  Search for the first position in b0 greater or equal than pos. 
 *  In which one of the characters from b1 is found and return it.  
 *  If such a position does not exist in b0, then BSTR_ERR is returned.
 * 
 */
int binchr (const bstring* b0, int pos, const bstring* b1) {
	struct charField chrs;
	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (pos >= b0->slen || b1->slen == 0)
		return BSTR_ERR;

	if (1 == b1->slen) 
		return bstrchrp (b0, b1->data[0], pos);
	
	buildCharField (&chrs, b1);
	return binchrCF (b0->data, b0->slen, pos, &chrs);
}

/* Inner engine for binchrr */
static int binchrrCF (const unsigned char * data, int pos,
                      const struct charField * cf) {
	int i;
	for (i=pos; i >= 0; i--) {
		unsigned int c = (unsigned int) data[i];
		if (testInCharField (cf, c)) 
			return i;
	}
	return BSTR_ERR;
}

/*  int binchrr (const bstring* b0, int pos, const bstring* b1);
 *
 *  Search for the last position in b0 less or equal than pos. 
 *  In which one of the characters from b1 is found and return it.  
 *  If such a position does not exist in b0, then BSTR_ERR is returned.
 * 
 *  TF: pos is clamped to b0->slen - 1
 *  
 */
int binchrr (const bstring* b0, int pos, const bstring* b1) {
	struct charField chrs;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (b1->slen == 0)
		return BSTR_ERR;

	if (b0->slen <= pos)
		pos = b0->slen - 1;

	if (1 == b1->slen) 
		return bstrrchrp (b0, b1->data[0], pos);
	
	buildCharField (&chrs, b1);
	return binchrrCF (b0->data, pos, &chrs);
}

/*  int bninchr (const bstring* b0, int pos, const bstring* b1);
 *
 *  Search for the first position in b0 greater or equal than pos, in which
 *  none of the characters from b1 are found and return it.  If such a position
 *  does not exist in b0, then BSTR_ERR is returned.
 *
 */
int bninchr (const bstring* b0, int pos, const bstring* b1) {
	struct charField chrs;

	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);


	if (pos >= b0->slen) 
		return BSTR_ERR;
	if (b1->slen == 0)
		return pos;
	
	buildCharField (&chrs, b1);
	invertCharField (&chrs);
	return binchrCF (b0->data, b0->slen, pos, &chrs);
}

/*  int bninchrr (const bstring* b0, int pos, const bstring* b1);
 *
 *  Search for the last position in b0 less or equal than pos, in which none of
 *  the characters in b1 is found and return it.  If such a position does not
 *  exist in b0, then BSTR_ERR is returned.
 * 
 *  TF: pos is clamped to b0->slen - 1
 */
int bninchrr (const bstring* b0, int pos, const bstring* b1) {
	struct charField chrs;
	BSTR_TEST_ASSERT(bconstisvalid(b0));
	ASSERT(bconstisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(b1));
	ASSERT(bconstisvalid(b1));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);

	if (b0->slen == 0)
		return BSTR_ERR;
	if (pos >= b0->slen)
		pos = b0->slen - 1;
	if (b1->slen == 0)
		return pos;
	
	buildCharField (&chrs, b1);
	invertCharField (&chrs);
	return binchrrCF (b0->data, pos, &chrs);
}

/*  int bsetstr (bstring b0, int pos, bstring b1, unsigned char fill)
 *
 *  Overwrite the string b0 starting at position pos with the string b1. If
 *  the position pos is past the end of b0, then the character "fill" is
 *  appended as necessary to make up the gap between the end of b0 and pos.
 *  If b1 is NULL, it behaves as if it were a 0-length string.
 */
BSTR_DECLARE_FN(int, bsetstr, bstring* b0, int pos, const bstring* b1, unsigned char fill) {
	int newlen, ret;
	ptrdiff_t pd;
	unsigned char auxBuf[BSTR_TMP_BUFFER_SIZE];
	bstring aux = b1 ? *b1 : bempty();

	BSTR_TEST_ASSERT(bisvalid(b0));
	ASSERT(bisvalid(b0));
	BSTR_TEST_ASSERT(bconstisvalid(&aux));
	ASSERT(bconstisvalid(&aux));
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);


	/* Aliasing */
	if (b1 && 
		(pd = (ptrdiff_t)(b1->data - b0->data)) >= 0 &&
		pd < (ptrdiff_t)bmlen(b0)) {

		if (b1->slen + 1 < BSTR_TMP_BUFFER_SIZE)
		{
			aux.data = &auxBuf[0];
			aux.mlen = BSTR_TMP_BUFFER_SIZE;
			aux.slen = 0;
		}
		else
			aux = bempty();

		ret = bassign(&aux, b1);
		ASSERT(ret == BSTR_OK);
	}

	ASSERT(INT_MAX - pos > aux.slen);
	newlen = pos + aux.slen;

	/* Increase memory size if necessary */
	ret = balloc(b0, newlen + 1);
	ASSERT(ret == BSTR_OK);

	/* Fill in "fill" character as necessary */
	if (pos > b0->slen) 
	{
		memset(b0->data + b0->slen, (int)fill,
			(size_t)(pos - b0->slen));
	}

	/* Copy b1 to position pos in b0. */
	memcpy(b0->data + pos, aux.data, aux.slen);

	b0->slen = newlen;
	ASSERT(bmlen(b0) > b0->slen);
	writeNull(b0, newlen);
	if (!b1 || aux.data != b1->data)
		bdestroy(&aux);

	return BSTR_OK;
}

/*  int binsertblk (bstring b, int pos, const void * blk, int len,
 *                  unsigned char fill)
 *
 *  Inserts the block of characters at blk with length len into b at position
 *  pos.  If the position pos is past the end of b, then the character "fill"
 *  is appended as necessary to make up the gap between the end of b1 and pos.
 *  Unlike bsetstr, binsert does not allow b2 to be NULL.
 */
BSTR_DECLARE_FN(int, binsertblk, bstring* b, int pos, const void * blk, int len, unsigned char fill) 
{
	int d, l, err;
	bstring aux = { 0, len, (unsigned char*)blk };

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(len >= 0 && blk != NULL);
	ASSERT(len >= 0 && blk != NULL);
	BSTR_TEST_ASSERT(pos >= 0);
	ASSERT(pos >= 0);
	int mlen = bmlen(b);

	/*
	 * The Forge: when conversion is allowed try
	 * storing temporary on the stack when aliasing occurs
	 */
	unsigned char auxBuffer[BSTR_TMP_BUFFER_SIZE] = "";


	/* Compute the two possible end pointers */
	d = b->slen + len;
	l = pos + len;
	BSTR_TEST_ASSERT((d | l) >= 0);
	ASSERT((d | l) >= 0); /* Integer wrap around. */

	/* Aliasing case */
	if (len != 0 &&
		aux.data + len >= b->data &&
	    aux.data < b->data + mlen) 
	{

		if (len + 1 <= BSTR_TMP_BUFFER_SIZE)
		{
			aux.data = (unsigned char*)auxBuffer;
			aux.mlen = BSTR_TMP_BUFFER_SIZE;
			aux.slen = 0;
		}
		else
		{
			aux = bempty();
		}

		err = bassignblk(&aux, blk, len);
		ASSERT(err == BSTR_OK);
	}

	if (l > d) {
		/* Inserting past the end of the string */
		err = balloc(b, l + 1);
		ASSERT(err == BSTR_OK);

		memset (b->data + b->slen, (int) fill,
		              (size_t) (pos - b->slen));
		b->slen = l;
	} else {
		/* Inserting in the middle of the string */
		err = balloc(b, d + 1);
		ASSERT(err == BSTR_OK);

		bBlockCopy (b->data + l, b->data + pos, d - l);
		b->slen = d;
	}
	bBlockCopy (b->data + pos, aux.data, len);
	ASSERT(bmlen(b) > b->slen);
	writeNull(b, b->slen);
	if (aux.data != blk)
		bdestroy(&aux);
	return BSTR_OK;
}

/*  int binsert (bstring b1, int pos, const bstring* b2, unsigned char fill)
 *
 *  Inserts the string b2 into b1 at position pos.  If the position pos is
 *  past the end of b1, then the character "fill" is appended as necessary to
 *  make up the gap between the end of b1 and pos.  Unlike bsetstr, binsert
 *  does not allow b2 to be NULL.
 */
BSTR_DECLARE_FN(int, binsert, bstring*  b1, int pos, const bstring* b2, unsigned char fill) {
	BSTR_TEST_ASSERT(b2);
	ASSERT(b2);
	return binsertblk(b1, pos, b2->data, b2->slen, fill);
}

/*  int breplace (bstring b1, int pos, int len, bstring b2,
 *                unsigned char fill)
 *
 *  Replace a section of a string from pos for a length len with the string
 *  b2. fill is used when pos > b1->slen.
 *  
 *  More efficient version of these steps:
 *  if (b1->slen > pos)
 *		bdelete(b1, pos, len);
 *  binsert(b1, pos, b2, fill);
 */
BSTR_DECLARE_FN(int, breplace, bstring* b1, int pos, int len, const bstring* b2, unsigned char fill) 
{
	int pl, ret;
	ptrdiff_t pd;
	bstring aux;

	/*
	 * The Forge: when conversion is allowed try
	 * storing temporary on the stack when aliasing occurs
	 */
	unsigned char auxBuffer[BSTR_TMP_BUFFER_SIZE] = "";


	BSTR_TEST_ASSERT(bisvalid(b1));
	ASSERT(bisvalid(b1));
	BSTR_TEST_ASSERT(bconstisvalid(b2));
	ASSERT(bconstisvalid(b2));

	BSTR_TEST_ASSERT(pos >= 0 && len >= 0);
	ASSERT(pos >= 0 && len >= 0);

	aux = *b2;
	/* straddles end or pos + len overflow */
	if (len >= INT_MAX - pos || (pl = pos + len) >= b1->slen)
		return bsetstr(b1, pos, b2, fill);
	

	/* Aliasing case */
	if ((pd = (ptrdiff_t)(b2->data - b1->data)) >= 0 &&
		pd < (ptrdiff_t)b1->slen)
	{

		if (b2->slen + 1 <= BSTR_TMP_BUFFER_SIZE)
		{
			aux.data = (unsigned char*)auxBuffer;
			aux.mlen = BSTR_TMP_BUFFER_SIZE;
			aux.slen = 0;
		}
		else
		{
			aux = bempty();
		}

		ret = bassign(&aux, b2);
		ASSERT(ret == BSTR_OK);
	}

	ASSERT(INT_MAX - aux.slen > b1->slen);

	if (bmlen(b1) <= b1->slen + aux.slen - len)
	{
		ret = balloc(b1, b1->slen + aux.slen - len + 1);
		ASSERT(ret == BSTR_OK);
	}

	if (aux.slen != len)
	{
		memmove(b1->data + pos + aux.slen,
			b1->data + pos + len,
			b1->slen - (pos + len));
	}

	memcpy (b1->data + pos, aux.data, aux.slen);
	b1->slen += aux.slen - len;
	ASSERT(bmlen(b1) > b1->slen);
	writeNull(b1, b1->slen);
	if (aux.data != b2->data)
		bdestroy (&aux);
	return BSTR_OK;
}

/*
 *  findreplaceengine is used to implement bfindreplace and
 *  bfindreplacecaseless. It works by breaking the three cases of
 *  expansion, reduction and replacement, and solving each of these
 *  in the most efficient way possible.
 */

typedef int (*instr_fnptr) (const bstring* s1, int pos, const bstring* s2);

#define INITIAL_STATIC_FIND_INDEX_COUNT 32

#define findreplaceengine(b, find, repl, pos, instr) BSTR_CALL(findreplaceengine, b, find, repl, pos, instr)

static BSTR_DECLARE_FN(int, findreplaceengine,
	bstring* b, const bstring* find,
	const bstring* repl, int pos,
    instr_fnptr instr) 
{
#ifdef ENABLE_MEMORY_TRACKING
	unsigned char fnNameBuf[128];
	bstring fnName = bemptyfromarr(fnNameBuf);
	bformat(&fnName, "%s(propagated from %s)", FUNCTION_NAME, PARENT_FUNCTION_NAME);
	ASSERT(!bownsdata(&fnName));
	const char* pFunction = (const char*)fnName.data;
#else
	#define FILE_NAME __FILE__
	#define FILE_LINE  __LINE__
	#define FUNCTION_NAME __FUNCTION__
	const char* pFunction = FUNCTION_NAME;
#endif

	int i, ret, slen, mlen, delta, acc;
	int * d;
	int static_d[INITIAL_STATIC_FIND_INDEX_COUNT+1]; /* This +1 is for LINT. */
	ptrdiff_t pd;

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(bconstisvalid(find));
	ASSERT(bconstisvalid(find));
	BSTR_TEST_ASSERT(bconstisvalid(repl));
	ASSERT(bconstisvalid(repl));
	BSTR_TEST_ASSERT(pos >= 0 && instr);
	ASSERT(pos >= 0 && instr);

	bstring auxf = *find;
	bstring auxr = *repl;

	/*
	 * The Forge: when conversion is allowed try
	 * storing temporary on the stack when aliasing occurs
	 */
	unsigned char auxfBuffer[BSTR_TMP_BUFFER_SIZE] = "";
	unsigned char auxrBuffer[BSTR_TMP_BUFFER_SIZE] = "";

	if (pos > b->slen - find->slen) return BSTR_OK;

	/* Alias with find string */
	pd = (ptrdiff_t) (find->data - b->data);
	if ((ptrdiff_t) (pos - find->slen) < pd && pd < (ptrdiff_t) b->slen) {

		if (find->slen + 1 <= BSTR_TMP_BUFFER_SIZE)
		{
			auxf.data = (unsigned char*)auxfBuffer;
			auxf.mlen = BSTR_TMP_BUFFER_SIZE;
			auxf.slen = 0;
		}
		else
		{
			auxf = bempty();
		}
		ret = bassign(&auxf, find);
		ASSERT(ret == BSTR_OK);
	}

	/* Alias with repl string */
	pd = (ptrdiff_t) (repl->data - b->data);
	if ((ptrdiff_t) (pos - repl->slen) < pd && pd < (ptrdiff_t) b->slen) {

		if (repl->slen + 1 <= BSTR_TMP_BUFFER_SIZE)
		{
			auxr.data = (unsigned char*)auxrBuffer;
			auxr.mlen = BSTR_TMP_BUFFER_SIZE;
			auxr.slen = 0;
		}
		else
		{
			auxr = bempty();
		}

		ret = bassign(&auxr, repl);
		ASSERT(ret == BSTR_OK);
	}

	delta = auxf.slen - auxr.slen;

	/* in-place replacement since find and replace strings are of equal
	   length */
	if (delta == 0) {
		while ((pos = instr (b, pos, &auxf)) >= 0) {
			memcpy (b->data + pos, auxr.data, auxr.slen);
			pos += auxf.slen;
		}
		if (auxf.data != find->data) 
			bdestroy (&auxf);
		if (auxr.data != repl->data) 
			bdestroy (&auxr);
		return BSTR_OK;
	}

	/* shrinking replacement since auxf->slen > auxr->slen */
	if (delta > 0) {
		acc = 0;

		while ((i = instr (b, pos, &auxf)) >= 0) {
			if (acc && i > pos)
				memmove (b->data + pos - acc, b->data + pos, i - pos);
			if (auxr.slen)
				memcpy (b->data + i - acc, auxr.data, auxr.slen);
			acc += delta;
			pos = i + auxf.slen;
		}

		if (acc) {
			i = b->slen;
			if (i > pos)
				memmove (b->data + pos - acc, b->data + pos, i - pos);
			b->slen -= acc;
			ASSERT(bmlen(b) > b->slen);
			writeNull(b, b->slen);
		}

		if (auxf.data != find->data) bdestroy (&auxf);
		if (auxr.data != repl->data) bdestroy (&auxr);
		return BSTR_OK;
	}

	/* expanding replacement since find->slen < repl->slen.  Its a lot
	   more complicated.  This works by first finding all the matches and
	   storing them to a growable array, then doing at most one resize of
	   the destination bstring and then performing the direct memory transfers
	   of the string segment pieces to form the final result. The growable
	   array of matches uses a deferred doubling reallocing strategy.  What
	   this means is that it starts as a reasonably fixed sized auto array in
	   the hopes that many if not most cases will never need to grow this
	   array.  But it switches as soon as the bounds of the array will be
	   exceeded.  An extra find result is always appended to this array that
	   corresponds to the end of the destination string, so slen is checked
	   against mlen - 1 rather than mlen before resizing.
	*/

	mlen = INITIAL_STATIC_FIND_INDEX_COUNT;
	d = (int *) static_d; /* Avoid malloc for trivial/initial cases */
	acc = slen = 0;

	while ((pos = instr (b, pos, &auxf)) >= 0) {
		if (slen >= mlen - 1) {
			int *t;
			int sl;
			/* Overflow */
			ASSERT(mlen <= (INT_MAX / sizeof(int *)) / 2);

			mlen += mlen;
			sl = sizeof (int *) * mlen;
			if (static_d == d) d = NULL; /* static_d cannot be realloced */
			t = (int *)tf_realloc_internal(d, sl, FILE_NAME, FILE_LINE, pFunction);
			ASSERT(t != NULL);
			if (NULL == d) memcpy (t, static_d, sizeof (static_d));
			d = t;
		}
		d[slen] = pos;
		slen++;
		acc -= delta;
		pos += auxf.slen;
		if (pos < 0 || acc < 0) {
			ret = BSTR_ERR;
			goto done;
		}
	}

	/* slen <= INITIAL_STATIC_INDEX_COUNT-1 or mlen-1 here. */
	d[slen] = b->slen;

	ret = balloc(b, b->slen + acc + 1);
	ASSERT(ret == BSTR_OK);

	b->slen += acc;
	for (i = slen-1; i >= 0; i--) {
		int s, l;
		s = d[i] + auxf.slen;
		l = d[i+1] - s; /* d[slen] may be accessed here. */
		if (l) {
			memmove (b->data + s + acc, b->data + s, l);
		}
		if (auxr.slen) {
			memmove(b->data + s + acc - auxr.slen,
			        auxr.data, auxr.slen);
		}
		acc += delta;
	}
	ASSERT(bmlen(b) > b->slen);
	writeNull(b, b->slen);

	done:;
	if (static_d != d) tf_free_internal(d, FILE_NAME, FILE_LINE, pFunction);
	if (auxf.data != find->data) bdestroy (&auxf);
	if (auxr.data != repl->data) bdestroy (&auxr);
	ASSERT(ret == BSTR_OK);
	return ret;
}

/*  int bfindreplace (bstring b, const bstring* find, const bstring* repl,
 *                    int pos)
 *
 *  Replace all occurrences of a find string with a replace string after a
 *  given point in a bstring.
 */
BSTR_DECLARE_FN(int, bfindreplace, bstring* b, const bstring* find, const bstring* repl, int pos) 
{
	return findreplaceengine (b, find, repl, pos, binstr);
}

/*  int bfindreplacecaseless (bstring b, const bstring* find,
 *                            const bstring* repl, int pos)
 *
 *  Replace all occurrences of a find string, ignoring case, with a replace
 *  string after a given point in a bstring.
 */
BSTR_DECLARE_FN(int, bfindreplacecaseless, bstring* b, const bstring* find, const bstring* repl, int pos) 
{
	return findreplaceengine (b, find, repl, pos, binstrcaseless);
}

/*  int binsertch (bstring b, int pos, int len, unsigned char fill)
 *
 *  Inserts the character fill repeatedly into b at position pos for a
 *  length len.  If the position pos is past the end of b, then the
 *  character "fill" is appended as necessary to make up the gap between the
 *  end of b and the position pos + len.
 */
BSTR_DECLARE_FN(int, binsertch, bstring* b, int pos, int len, unsigned char fill) {
	int d, l, i, ret;

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(pos >= 0 && len >= 0);
	ASSERT(pos >= 0 && len >= 0);

	BSTR_TEST_ASSERT(len < INT_MAX - b->slen);
	ASSERT(len < INT_MAX - b->slen);
	BSTR_TEST_ASSERT(len < INT_MAX - pos);
	ASSERT(len < INT_MAX - pos);

	/* Compute the two possible end pointers */
	d = b->slen + len;
	l = pos + len;

	if (l > d) {
		/* Inserting past the end of the string */
		ret = balloc(b, l + 1);
		ASSERT(ret == BSTR_OK);
		pos = b->slen;
		b->slen = l;
	} else {
		/* Inserting in the middle of the string */
		ret = balloc(b, d + 1);
		ASSERT(ret == BSTR_OK);
		for (i = d - 1; i >= l; i--) {
			b->data[i] = b->data[i - len];
		}
		b->slen = d;
	}

	for (i=pos; i < l; i++) 
		b->data[i] = fill;
	ASSERT(bmlen(b) > b->slen);
	writeNull(b, b->slen);
	return BSTR_OK;
}

/*  int bpattern (bstring b, int len)
 *
 *  Replicate the bstring, b in place, end to end repeatedly until it
 *  surpasses len characters, then chop the result to exactly len characters.
 *  This function operates in-place.  The function will return with BSTR_ERR
 *  if b is NULL or of length 0, otherwise BSTR_OK is returned.
 */
BSTR_DECLARE_FN(int, bpattern, bstring* b, int len) {
	int i, d;

	BSTR_TEST_ASSERT(bisvalid(b));
	ASSERT(bisvalid(b));
	BSTR_TEST_ASSERT(len >= 0);
	ASSERT(len >= 0);

	d = b->slen;
	if (d == 0)
		return BSTR_ERR;

	ASSERT(len < INT_MAX);

	int allocResult = balloc(b, len + 1);
	ASSERT(allocResult == BSTR_OK);

	if (len > 0) {
		if (d == 1)
			return binsertch(b, len, 0, b->data[0]);

		for (i = d; i < len; i++) 
			b->data[i] = b->data[i - d];
	}
	ASSERT(bmlen(b) > len);
	writeNull(b, len);
	b->slen = len;
	return BSTR_OK;
}



/*  bstring bjoinblk (const struct bstrList * bl, void * blk, int len);
 *
 *  Join the entries of a bstrList into one bstring by sequentially
 *  concatenating them with the content from blk for length len in between.
 *  If there is an error NULL is returned, otherwise a bstring with the
 *  correct result is returned.
 */
BSTR_DECLARE_FN(int, bjoinblk, bstring* out, const bstring inputs[], int count, const void * sep, int sepLen) {
	unsigned char* p;
	int i, c, v;

	BSTR_TEST_ASSERT(bisvalid(out));
	ASSERT(bisvalid(out));
	BSTR_TEST_ASSERT(count >= 0 && (inputs != NULL || count == 0));
	ASSERT(count >= 0 && (inputs != NULL || count == 0));
	BSTR_TEST_ASSERT(sepLen >= 0 && (sep != NULL || sepLen == 0));
	ASSERT(sepLen >= 0 && (sep != NULL || sepLen == 0));

	if (count == 0)
		return bassignblk(out, "", 0);
	
	bool hasAliasing = bstrIntersects(out, sep);

	for (i = 0, c = 1; i < count; i++) {
		BSTR_TEST_ASSERT(bconstisvalid(&inputs[i]));
		ASSERT(bconstisvalid(&inputs[i]));
		
		hasAliasing = hasAliasing || bstrIntersects(out, inputs[i].data);
		
		v = inputs[i].slen;
		BSTR_TEST_ASSERT(v <= INT_MAX - c);
		ASSERT(v <= INT_MAX - c);
		c += v;
	}
	
	if (c == 1 && sepLen == 0)
	{
		bassignblk(out, "", 0);
		return BSTR_OK;
	}
		
	bstring oldOut = *out;

	if (hasAliasing)
		*out = bempty();

	if (sepLen == 0) {
		int allocResult = balloc(out, c);
		ASSERT(allocResult == BSTR_OK);
		
		p = out->data;

		for (i = 0; i < count; i++) {
			v = inputs[i].slen;
			if (v)
				memcpy(p, inputs[i].data, v);
			p += v;
		}
	} else {
		v = (count - 1) * sepLen;
		/* Overflow */
		BSTR_TEST_ASSERT(
			(count <= 512 && sepLen <= 127) ||
			(v / sepLen == count - 1)
		);
		ASSERT(
			(count <= 512 && sepLen <= 127) ||
			(v / sepLen == count - 1)
		);

		BSTR_TEST_ASSERT(v <= INT_MAX - c);
		ASSERT(v <= INT_MAX - c);
		
		c += v;

		int allocResult = balloc(out, c);
		ASSERT(allocResult == BSTR_OK);
		p = out->data;

		v = inputs[0].slen;
		if (v)
		{
			memcpy(p, inputs[0].data, v);
			p += v;
		}

		for (i = 1; i < count; i++) {
			memcpy (p, sep, sepLen);
			p += sepLen;
			v = inputs[i].slen;
			if (v) {
				memcpy (p, inputs[i].data, v);
				p += v;
			}
		}
	}

	out->slen = c-1;
	ASSERT(bmlen(out) > (c-1));
	writeNull(out, c - 1);
	if (hasAliasing)
	{
		// Copy result into existing memory if possible and destroy allocated
		if (bmlen(&oldOut) > out->slen)
		{
			bassign(&oldOut, out);
			bdestroy(out);
			*out = oldOut;
		}
		// Otherwise make sure that there is no memory leak
		else
			bdestroy(&oldOut);
	}
	return BSTR_OK;
}

/*  bstring bjoin (const struct bstrList * bl, const bstring* sep);
 *
 *  Join the entries of a bstrList into one bstring by sequentially
 *  concatenating them with the sep string in between.  If there is an error
 *  NULL is returned, otherwise a bstring with the correct result is returned.
 */
BSTR_DECLARE_FN(int, bjoin, bstring* out, const bstring inputs[], int count, const bstring* sep) {

	const bstring empty = bempty();
	if (sep == NULL)
		sep = &empty;
	BSTR_TEST_ASSERT(bconstisvalid(sep));
	ASSERT(bconstisvalid(sep));
	return bjoinblk(out, inputs, count, sep->data, sep->slen);
}

/*  int bsplitcb (const bstring* str, unsigned char splitChar, int pos,
 *                int (* cb) (void * parm, int ofs, int len), void * parm)
 *
 *  Iterate the set of disjoint sequential substrings over str divided by the
 *  character in splitChar.
 *
 *  Note: Non-destructive modification of str from within the cb function
 *  while performing this split is not undefined.  bsplitcb behaves in
 *  sequential lock step with calls to cb.  I.e., after returning from a cb
 *  that return a non-negative integer, bsplitcb continues from the position
 *  1 character after the last detected split character and it will halt
 *  immediately if the length of str falls below this point.  However, if the
 *  cb function destroys str, then it *must* return with a negative value,
 *  otherwise bsplitcb will continue in an undefined manner.
 */
int bsplitcb (const bstring* str, unsigned char splitChar, int pos,
	BSplitCallbackFn cb, void * parm) {
	int i, p, ret;

	BSTR_TEST_ASSERT(str && bconstisvalid(str));
	ASSERT(str && bconstisvalid(str));
	BSTR_TEST_ASSERT(cb && pos >= 0 && pos <= str->slen);
	ASSERT(cb && pos >= 0 && pos <= str->slen);

	p = pos;
	do {
		for (i=p; i < str->slen; i++) {
			if (str->data[i] == splitChar) break;
		}
		if ((ret = cb (parm, p, i - p)) < 0) return ret;
		p = i + 1;
	} while (p <= str->slen);
	return BSTR_OK;
}

/*  int bsplitscb (const bstring* str, const bstring* splitStr, int pos,
 *                 int (* cb) (void * parm, int ofs, int len), void * parm)
 *
 *  Iterate the set of disjoint sequential substrings over str divided by any
 *  of the characters in splitStr.  An empty splitStr causes the whole str to
 *  be iterated once.
 *
 *  Note: Non-destructive modification of str from within the cb function
 *  while performing this split is not undefined.  bsplitscb behaves in
 *  sequential lock step with calls to cb.  I.e., after returning from a cb
 *  that return a non-negative integer, bsplitscb continues from the position
 *  1 character after the last detected split character and it will halt
 *  immediately if the length of str falls below this point.  However, if the
 *  cb function destroys str, then it *must* return with a negative value,
 *  otherwise bsplitscb will continue in an undefined manner.
 */
int bsplitscb (const bstring* str, const bstring* splitChars, int pos,
	BSplitCallbackFn cb, void * parm) {
	struct charField chrs;
	int i, p, ret;

	BSTR_TEST_ASSERT(str && bconstisvalid(str));
	ASSERT(str && bconstisvalid(str));
	BSTR_TEST_ASSERT(splitChars && bconstisvalid(splitChars));
	ASSERT(splitChars && bconstisvalid(splitChars));
	BSTR_TEST_ASSERT(cb && pos >= 0 && pos <= str->slen);
	ASSERT(cb && pos >= 0 && pos <= str->slen);


	if (splitChars->slen == 0) {
		if ((ret = cb (parm, 0, str->slen)) > 0) ret = 0;
		return ret;
	}

	if (splitChars->slen == 1)
		return bsplitcb (str, splitChars->data[0], pos, cb, parm);

	buildCharField (&chrs, splitChars);

	p = pos;
	do {
		for (i=p; i < str->slen; i++) {
			if (testInCharField (&chrs, str->data[i])) break;
		}
		if ((ret = cb (parm, p, i - p)) < 0) return ret;
		p = i + 1;
	} while (p <= str->slen);
	return BSTR_OK;
}

/*  int bsplitstrcb (const bstring* str, const bstring* splitStr, int pos,
 *	int (* cb) (void * parm, int ofs, int len), void * parm)
 *
 *  Iterate the set of disjoint sequential substrings over str divided by the
 *  substring splitStr.  An empty splitStr causes the whole str to be
 *  iterated once.
 *
 *  Note: Non-destructive modification of str from within the cb function
 *  while performing this split is not undefined.  bsplitstrcb behaves in
 *  sequential lock step with calls to cb.  I.e., after returning from a cb
 *  that return a non-negative integer, bsplitscb continues from the position
 *  1 character after the last detected split character and it will halt
 *  immediately if the length of str falls below this point.  However, if the
 *  cb function destroys str, then it *must* return with a negative value,
 *  otherwise bsplitscb will continue in an undefined manner.
 */
int bsplitstrcb (const bstring* str, const bstring* splitStr, int pos,
	BSplitCallbackFn cb, void * parm) {
	int i, p, ret;

	BSTR_TEST_ASSERT(str && bconstisvalid(str));
	ASSERT(str && bconstisvalid(str));
	BSTR_TEST_ASSERT(splitStr && bconstisvalid(splitStr));
	ASSERT(splitStr && bconstisvalid(splitStr));
	BSTR_TEST_ASSERT(cb && pos >= 0 && pos <= str->slen);
	ASSERT(cb && pos >= 0 && pos <= str->slen);

	if (splitStr->slen == 0) {
		if (str->slen == 0) return cb(parm, 0, 0);

		for (i=pos; i < str->slen; i++) {
			if ((ret = cb (parm, i, 1)) < 0) return ret;
		}
		return BSTR_OK;
	}

	if (splitStr->slen == 1)
		return bsplitcb (str, splitStr->data[0], pos, cb, parm);

	for (i=p=pos; i <= str->slen - splitStr->slen; i++) {
		if (0 == memcmp (splitStr->data, str->data + i,
		                       splitStr->slen)) {
			if ((ret = cb (parm, p, i - p)) < 0) return ret;
			i += splitStr->slen;
			p = i;
		}
	}
	if ((ret = cb (parm, p, str->slen - p)) < 0) return ret;
	return BSTR_OK;
}

typedef struct SplitStrParams
{
	const bstring* b;
	bstring* arr;
}SplitStrParams;

static int bscb(void* parm, int ofs, int len)
{
	SplitStrParams* params = (SplitStrParams*)parm;

	ASSERT(
		ofs >= 0 && len >= 0 &&
		ofs <= params->b->slen && (ofs + len) <= params->b->slen
	);

	const bstring* str = params->b;
	arrsetlen(params->arr, arrlen(params->arr) + 1);
	bstring* substr = arrback(params->arr);
	*substr = bempty();

	int ret = BSTR_CALL_NO_TRACKING(bassignblk, substr, str->data + ofs, len);
	ASSERT(ret == BSTR_OK);

	return BSTR_OK;
}

#define bscb(parm, ofs, len) BSTR_CALL(bscb, parm, ofs, len)

/*  struct bstrList * bsplit (const bstring* str, unsigned char splitChar)
 *
 *  Create an array of sequential substrings from str divided by the character
 *  splitChar.
 * 
 *  Always writes number of found substrings into count
 *  If substrings is not NULL copies at most count substrings into substrings array
 *    Note: Does early termination if error occurs(e.g. size of array is not big enough)
 *
 *  Returns BSTR_OK on success
 *  If error occurs, the logic is following:
 *  offset = the first character of the first substring on which error occured
 *  + if offset > 0 returns negative offset
 *  + if offset == 0 returns INT_MIN
 */
BSTR_DECLARE_FN(bstring*, bsplit, const bstring* str, unsigned char splitChar)
{
	#ifdef ENABLE_MEMORY_TRACKING
    UNREF_PARAM(PARENT_FUNCTION_NAME);
	UNREF_PARAM(FUNCTION_NAME);
	UNREF_PARAM(FILE_NAME);
	UNREF_PARAM(FILE_LINE);
    #endif
	BSTR_TEST_ASSERT_RET(bconstisvalid(str), NULL);
	ASSERT(bconstisvalid(str));

	SplitStrParams splitParams;

	splitParams.b = str;
	splitParams.arr = NULL;

	int ret = bsplitcb(str, splitChar, 0, bscb, &splitParams);
	BSTR_TEST_ASSERT_RET(ret == BSTR_OK, (arrfree(splitParams.arr), NULL));
	ASSERT(ret == BSTR_OK);

	return splitParams.arr;
}

/*  struct bstrList * bsplitstr (const bstring* str, const bstring* splitStr)
 *
 *  Create an array of sequential substrings from str divided by the entire
 *  substring splitStr.
 *
 *
 *  Always writes number of found substrings into count
 *  If substrings is not NULL copies at most count substrings into substrings array
 *    Note: Does early termination if error occurs(e.g. size of array is not big enough)
 *
 *  Returns BSTR_OK on success
 *  If error occurs, the logic is following:
 *  offset = the first character of the first substring on which error occured
 *  + if offset > 0 returns negative offset
 *  + if offset == 0 returns INT_MIN
 */
BSTR_DECLARE_FN(bstring*, bsplitstr, const bstring* str, const bstring* splitStr)
{
	#ifdef ENABLE_MEMORY_TRACKING
    UNREF_PARAM(PARENT_FUNCTION_NAME);
	UNREF_PARAM(FUNCTION_NAME);
	UNREF_PARAM(FILE_NAME);
	UNREF_PARAM(FILE_LINE);
    #endif
	BSTR_TEST_ASSERT_RET(bconstisvalid(str), NULL);
	ASSERT(bconstisvalid(str));
	BSTR_TEST_ASSERT_RET(bconstisvalid(splitStr), NULL);
	ASSERT(bconstisvalid(splitStr));

	SplitStrParams splitParams;

	splitParams.b = str;
	splitParams.arr = NULL;

	int ret = bsplitstrcb(str, splitStr, 0, bscb, &splitParams);
	BSTR_TEST_ASSERT_RET(ret == BSTR_OK, (arrfree(splitParams.arr), NULL));
	ASSERT(ret == BSTR_OK);

	return splitParams.arr;
}

/*  struct bstrList * bsplits (const bstring* str, bstring splitStr)
 *
 *  Create an array of sequential substrings from str divided by any of the
 *  characters in splitStr.  An empty splitStr causes a single entry bstrList
 *  containing a copy of str to be returned.
 */
BSTR_DECLARE_FN(bstring*, bsplits, const bstring* str, const bstring* splitChars)
{
	#ifdef ENABLE_MEMORY_TRACKING
    UNREF_PARAM(PARENT_FUNCTION_NAME);
	UNREF_PARAM(FUNCTION_NAME);
	UNREF_PARAM(FILE_NAME);
	UNREF_PARAM(FILE_LINE);
    #endif
	BSTR_TEST_ASSERT_RET(bconstisvalid(str), NULL);
	ASSERT(bconstisvalid(str));

	SplitStrParams splitParams;

	splitParams.b = str;
	splitParams.arr = NULL;

	int ret = bsplitscb(str, splitChars, 0, bscb, &splitParams);
	BSTR_TEST_ASSERT_RET(ret == BSTR_OK, (arrfree(splitParams.arr), NULL));
	ASSERT(ret == BSTR_OK);

	return splitParams.arr;
}

#if !defined (BSTRLIB_NOVSNP)

#if defined(__WATCOMC__) || (defined(_MSC_VER) && _MSC_VER < 1900) || (defined(__TURBOC__) && !defined (__BORLANDC__))
#error "vsnprintf is not properly supported"
#endif

typedef enum LengthModifier
{
	MODIFIER_NONE = 0,
	MODIFIER_HH = -2,
	MODIFIER_H = -1,
	MODIFIER_L = 1,
	MODIFIER_LL = 2,
	MODIFIER_J = 4,
	MODIFIER_Z = 8,
	MODIFIER_T = 16,
	MODIFIER_BIG_L = 32
}LengthModifier;

/*
 * Helper function for breaking out of the loop in bstrIntersectsVaList
 * returns true if current symbol represents any length modifier and sets pModifier
 */
static inline bool modifyVariableLength(LengthModifier* pModifier, char symbol)
{
	ASSERT(pModifier);
	switch (symbol) {
	case 'h':
		ASSERT(*pModifier == 0 || *pModifier == -1);
		--*pModifier;
		return true;
	case 'l':
		ASSERT(*pModifier == 0 || *pModifier == 1);
		++*pModifier;
		return true;
	case 'j':
		ASSERT(*pModifier == 0);
		*pModifier = MODIFIER_J;
		return true;
	case 'z':
		ASSERT(*pModifier == 0);
		*pModifier = MODIFIER_Z;
		return true;
	case 't':
		ASSERT(*pModifier == 0);
		*pModifier = MODIFIER_T;
		return true;
	case 'L':
		ASSERT(*pModifier == 0);
		*pModifier = MODIFIER_BIG_L;
		return true;
	}
	return false;
}


/*
 * Returns true if any arguments from arglist intersect str buffer
 * returns false otherwise
 *
 * Note: doesn't change modify args
 */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#endif
static inline bool bstrIntersectsVaList(bstring* str, const char* fmt, va_list args)
{
	static const char formatSpecifiers[] = "%csdioxXufFeEaAgGnp";

	ASSERT(fmt);
	va_list args1;
	va_copy(args1, args);

	for (; *fmt; ++fmt)
	{
		if (*fmt == '%')
		{
			bool found = false;

			LengthModifier lengthModifier = 0;

			/* Find argument type and length modifier*/
			do
			{
				++fmt;
				for (int i = 0; i < sizeof(formatSpecifiers) / sizeof(formatSpecifiers[0]); ++i)
				{
					found = found || *fmt == formatSpecifiers[i];
					if (modifyVariableLength(&lengthModifier, *fmt))
						break;
				}
					
			} while (!found);

			/* 
			 * Pop input argument 
			 * For strings check if they intersect the buffer
			 */
			switch (*fmt)
			{
			case 's':
				switch (lengthModifier)
				{
				case MODIFIER_NONE:
				{
					const char* p = va_arg(args1, const char*);

					/* empty string buffer intersection doesn't matter */
					if (bstrIntersects(str, p) && p != gEmptyStringBuffer)
					{
						va_end(args1);
						return true;
					}

					break;
				}
				case MODIFIER_L:
					va_arg(args1, wchar_t*);
					break;
				default:
					ASSERT(0 && "Invalid format");
				}
			case '%':
				ASSERT(lengthModifier == 0 && "Invalid format");
				break;
			case 'c':
				switch (lengthModifier)
				{
				case MODIFIER_NONE:
					va_arg(args1, int);
					break;
				case MODIFIER_L:
					va_arg(args1, wint_t);
					break;
				default:
					ASSERT(0 && "Invalid format");
				}
			case 'd':
			case 'i':
				switch (lengthModifier)
				{

				case MODIFIER_HH:
				case MODIFIER_H:
				case MODIFIER_NONE:
					va_arg(args1, int);
					break;
				case MODIFIER_L:
					va_arg(args1, long);
					break;
				case MODIFIER_LL:
					va_arg(args1, long long);
					break;
				case MODIFIER_J:
					va_arg(args1, intmax_t);
					break;
				case MODIFIER_Z:
					va_arg(args1, size_t);
					break;
				case MODIFIER_T:
					va_arg(args1, ptrdiff_t);
					break;
				default:
					ASSERT(0 && "Invalid format");
				}
				break;
			case 'o':
			case 'x':
			case 'X':
			case 'u':
				switch (lengthModifier)
				{

				case MODIFIER_HH:
				case MODIFIER_H:
				case MODIFIER_NONE:
					va_arg(args1, unsigned int);
					break;
				case MODIFIER_L:
					va_arg(args1, unsigned long);
					break;
				case MODIFIER_LL:
					va_arg(args1, unsigned long long);
					break;
				case MODIFIER_J:
					va_arg(args1, uintmax_t);
					break;
				case MODIFIER_Z:
					va_arg(args1, size_t);
					break;
				case MODIFIER_T:
					va_arg(args1, ptrdiff_t);
					break;
				default:
					ASSERT(0 && "Invalid format");
				}
				break;
			case 'f':
			case 'F':
			case 'e':
			case 'E':
			case 'a':
			case 'A':
			case 'g':
			case 'G':
				switch (lengthModifier)
				{
				case MODIFIER_NONE:
				case MODIFIER_L:
					va_arg(args1, double);
					break;
				case MODIFIER_BIG_L:
					va_arg(args1, long double);
					break;
				default:
					ASSERT(0 && "Invalid format");
				}
				break;
			case 'n':
				switch (lengthModifier)
				{
				case MODIFIER_NONE:
				case MODIFIER_HH:
				case MODIFIER_H:
				case MODIFIER_L:
				case MODIFIER_LL:
				case MODIFIER_J:
				case MODIFIER_Z:
				case MODIFIER_T:
					va_arg(args1, void*);
					break;
				default:
					ASSERT(0 && "Invalid format");
				}
			case 'p':
				ASSERT(lengthModifier == MODIFIER_NONE && "Invalid format");
				va_arg(args1, void*);
				break;
			default:
				ASSERT(0 && "Invalid format");

			}
        }
	}

	va_end(args1);
	return false;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif


BSTR_DECLARE_FN(int, bvformata, bstring*  b, const char* fmt, va_list args)
{
	va_list args1;

	BSTR_TEST_ASSERT(fmt && bisvalid(b));
	ASSERT(fmt && bisvalid(b));
	BSTR_TEST_ASSERT(!bstrIntersectsVaList(b, fmt, args));
	ASSERT(!bstrIntersectsVaList(b, fmt, args));

	va_copy(args1, args);

	int requiredSize = vsnprintf(NULL, 0, fmt, args1);
	va_end(args1);

	BSTR_TEST_ASSERT(requiredSize >= 0);
	ASSERT(requiredSize >= 0);

	if (requiredSize == 0)
	{
		int ret = balloc(b, b->slen + 1);
		ASSERT(ret == BSTR_OK);
		writeNull(b, b->slen);
		return ret;
	}

	int allocResult = balloc(b, b->slen + requiredSize + 1);
	ASSERT(allocResult == BSTR_OK);

	/* We know that capacity is sufficient*/
	vsprintf((char*)b->data + b->slen, fmt, args);
	b->slen = b->slen + requiredSize;
	return BSTR_OK;
}

BSTR_DECLARE_FN(int, bvformat, bstring*  b, const char* fmt, va_list args)
{
	va_list args1;

	BSTR_TEST_ASSERT(fmt && bisvalid(b));
	ASSERT(fmt && bisvalid(b));
	BSTR_TEST_ASSERT(!bstrIntersectsVaList(b, fmt, args));
	ASSERT(!bstrIntersectsVaList(b, fmt, args));

	va_copy(args1, args);

	int requiredSize = vsnprintf(NULL, 0, fmt, args1);
	va_end(args1);

	BSTR_TEST_ASSERT(requiredSize >= 0);
	ASSERT(requiredSize >= 0);

	if (requiredSize == 0)
	{
		return bassignblk(b, "", 0);
	}

	int allocResult = balloc(b, requiredSize + 1);
	ASSERT(allocResult == BSTR_OK);

	/* We know that capacity is sufficient*/
	vsprintf((char*)b->data, fmt, args);
	b->slen = requiredSize;
	return BSTR_OK;
}



/*  int bformata (bstring b, const char * fmt, ...)
 *
 *  After the first parameter, it takes the same parameters as printf (), but
 *  rather than outputting results to stdio, it appends the results to
 *  a bstring which contains what would have been output. Note that if there
 *  is an early generation of a '\0' character, the bstring will be truncated
 *  to this end point.
 */
BSTR_DECLARE_FN(int, bformata, bstring*  b, const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int ret = bvformata(b, fmt, args);
	va_end(args);
	return ret;
}

/*  int bformat (bstring b, const char * fmt, ...)
 *
 *  After the first parameter, it takes the same parameters as printf (), but
 *  rather than outputting results to stdio, it outputs the results to
 *  the bstring parameter b. Note that if there is an early generation of a
 *  '\0' character, the bstring will be truncated to this end point.
 */
BSTR_DECLARE_FN(int, bformat, bstring*  b, const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int ret = bvformat(b, fmt, args);
	va_end(args);
	return ret;
}



/* Functions removed by The Forge */
#if 0

/*  bstring bmidstr (const bstring* b, int left, int len)
 *
 *  Create a bstring which is the substring of b starting from position left
 *  and running for a length len (clamped by the end of the bstring b.)  If
 *  b is detectably invalid, then NULL is returned.  The section described
 *  by (left, len) is clamped to the boundaries of b.
 */
bstring* bmidstr(const bstring* b, int left, int len) {

	if (b == NULL || b->slen < 0 || b->data == NULL) return NULL;

	if (left < 0) {
		len += left;
		left = 0;
	}

	if (len > b->slen - left) len = b->slen - left;

	if (len <= 0) return bfromcstr("");
	return blk2bstr(b->data + left, len);
}

/*  bstring bfromcstr (const char * str)
 *
 *  Create a bstring which contains the contents of the '\0' terminated char *
 *  buffer str.
 */
bstring* bfromcstr(const char * str) {
	bstring* b;
	int i;
	size_t j;

	if (str == NULL) return NULL;
	j = (strlen)(str);
	i = snapUpSize((int)(j + (2 - (j != 0))));
	if (i <= (int)j) return NULL;

	b = (bstring*)tf_malloc(sizeof(struct tagbstring));
	if (NULL == b) return NULL;
	b->slen = (int)j;
	if (NULL == (b->data = (unsigned char *)tf_malloc(b->mlen = i))) {
		tf_free(b);
		return NULL;
	}

	memcpy(b->data, str, j + 1);
	return b;
}

/*  bstring bfromcstrrangealloc (int minl, int maxl, const char* str)
 *
 *  Create a bstring which contains the contents of the '\0' terminated
 *  char* buffer str.  The memory buffer backing the string is at least
 *  minl characters in length, but an attempt is made to allocate up to
 *  maxl characters.
 */
bstring* bfromcstrrangealloc(int minl, int maxl, const char* str) {
	bstring* b;
	int i;
	size_t j;

	/* Bad parameters? */
	if (str == NULL) return NULL;
	if (maxl < minl || minl < 0) return NULL;

	/* Adjust lengths */
	j = (strlen)(str);
	if ((size_t)minl < (j + 1)) minl = (int)(j + 1);
	if (maxl < minl) maxl = minl;
	i = maxl;

	b = (bstring*)tf_malloc(sizeof(struct tagbstring));
	if (b == NULL) return NULL;
	b->slen = (int)j;

	while (NULL == (b->data = (unsigned char *)tf_malloc(b->mlen = i))) {
		int k = (i >> 1) + (minl >> 1);
		if (i == k || i < minl) {
			tf_free(b);
			return NULL;
		}
		i = k;
	}

	memcpy(b->data, str, j + 1);
	return b;
}

/*  bstring bfromcstralloc (int mlen, const char * str)
 *
 *  Create a bstring which contains the contents of the '\0' terminated
 *  char* buffer str.  The memory buffer backing the string is at least
 *  mlen characters in length.
 */
bstring* bfromcstralloc(int mlen, const char * str) {
	return bfromcstrrangealloc(mlen, mlen, str);
}

/*  bstring blk2bstr (const void * blk, int len)
 *
 *  Create a bstring which contains the content of the block blk of length
 *  len.
 */
bstring* blk2bstr(const void * blk, int len) {
	bstring* b;
	int i;

	if (blk == NULL || len < 0) return NULL;
	b = (bstring*)tf_malloc(sizeof(struct tagbstring));
	if (b == NULL) return NULL;
	b->slen = len;

	i = len + (2 - (len != 0));
	i = snapUpSize(i);

	b->mlen = i;

	b->data = (unsigned char *)tf_malloc((size_t)b->mlen);
	if (b->data == NULL) {
		tf_free(b);
		return NULL;
	}

	if (len > 0) memcpy(b->data, blk, (size_t)len);
	writeNull(b, len);

	return b;
}

/*  char * bstr2cstr (const bstring* s, char z)
 *
 *  Create a '\0' terminated char * buffer which is equal to the contents of
 *  the bstring s, except that any contained '\0' characters are converted
 *  to the character in z. This returned value should be freed with a
 *  bcstrfree () call, by the calling application.
 */
char * bstr2cstr(const bstring* b, char z) {
	int i, l;
	char * r;

	if (b == NULL || b->slen < 0 || b->data == NULL) return NULL;
	l = b->slen;
	r = (char *)tf_malloc((size_t)(l + 1));
	if (r == NULL) return r;

	for (i = 0; i < l; i++) {
		r[i] = (char)((b->data[i] == '\0') ? z : (char)(b->data[i]));
	}

	r[l] = (unsigned char) '\0';

	return r;
}

/*  int bcstrfree (char * s)
 *
 *  Frees a C-string generated by bstr2cstr ().  This is normally unnecessary
 *  since it just wraps a call to tf_free (), however, if tf_malloc ()
 *  and tf_free () have been redefined as a macros within the bstrlib
 *  module (via defining them in memdbg.h after defining
 *  BSTRLIB_MEMORY_DEBUG) with some difference in behaviour from the std
 *  library functions, then this allows a correct way of freeing the memory
 *  that allows higher level code to be independent from these macro
 *  redefinitions.
 */
int bcstrfree(char * s) {
	if (s) {
		tf_free(s);
		return BSTR_OK;
	}
	return BSTR_ERR;
}
/*  bstring bstrcpy (const bstring* b)
 *
 *  Create a copy of the bstring b.
 */
bstring* bstrcpy(const bstring* b) {
	bstring* b0;
	int i, j;

	/* Attempted to copy an invalid string? */
	if (b == NULL || b->slen < 0 || b->data == NULL) return NULL;

	b0 = (bstring*)tf_malloc(sizeof(struct tagbstring));
	if (b0 == NULL) {
		/* Unable to allocate memory for string header */
		return NULL;
	}

	i = b->slen;
	j = snapUpSize(i + 1);

	b0->data = (unsigned char *)tf_malloc(j);
	if (b0->data == NULL) {
		j = i + 1;
		b0->data = (unsigned char *)tf_malloc(j);
		if (b0->data == NULL) {
			/* Unable to allocate memory for string data */
			tf_free(b0);
			return NULL;
		}
	}

	b0->mlen = j;
	b0->slen = i;

	if (i) memcpy((char *)b0->data, (char *)b->data, i);
	writeNull(b0, b0->slen);

	return b0;
}


#define BS_BUFF_SZ (1024)

/*  int breada (bstring b, bNread readPtr, void * parm)
 *
 *  Use a finite buffer fread-like function readPtr to concatenate to the
 *  bstring b the entire contents of file-like source data in a roughly
 *  efficient way.
 */
int breada(bstring* b, bNread readPtr, void * parm) {
	int i, l, n, ret;

	BSTR_TEST_ASSERT(b && bisvalid(b));
	ASSERT(b && bisvalid(b));
	BSTR_TEST_ASSERT(readPtr != NULL);
	ASSERT(readPtr != NULL);

	i = b->slen;
	for (n = i + 16; ; n += ((n < BS_BUFF_SZ) ? n : BS_BUFF_SZ)) {
		ret = balloc(b, n + 1);
		if (ret != BSTR_OK)
			return ret;

		l = (int)readPtr((void *)(b->data + i), 1, n - i, parm);
		i += l;
		b->slen = i;
		if (i < n) break;
	}

	writeNull(b, i);
	return BSTR_OK;
}

/*  int bassigngets (bstring b, bNgetc getcPtr, void * parm, char terminator)
 *
 *  Use an fgetc-like single character stream reading function (getcPtr) to
 *  obtain a sequence of characters which are concatenated to the end of the
 *  bstring b.  The stream read is terminated by the passed in terminator
 *  parameter.
 *
 *  If getcPtr returns with a negative number, or the terminator character
 *  (which is appended) is read, then the stream reading is halted and the
 *  function returns with a partial result in b.  If there is an empty partial
 *  result, 1 is returned.  If no characters are read, or there is some other
 *  detectable error, BSTR_ERR is returned.
 */
int bassigngets(bstring* b, bNgetc getcPtr, void * parm, char terminator) {
	int c, d, e;

	BSTR_TEST_ASSERT(b && bisvalid(b));
	ASSERT(b && bisvalid(b));
	BSTR_TEST_ASSERT(getcPtr != NULL);
	ASSERT(getcPtr != NULL);

	d = 0;
	e = b->mlen - 2;

	while ((c = getcPtr(parm)) >= 0) {
		if (d > e) {
			b->slen = d;
			int allocResult = balloc(b, d + 2);
			ASSERT(allocResult == BSTR_OK);
			e = b->mlen - 2;
		}
		b->data[d] = (unsigned char)c;
		d++;
		if (c == terminator) break;
	}

	if (b->data)
		writeNull(b, d);

	b->slen = d;

	return d == 0 && c < 0;
}

/*  int bgetsa (bstring b, bNgetc getcPtr, void * parm, char terminator)
 *
 *  Use an fgetc-like single character stream reading function (getcPtr) to
 *  obtain a sequence of characters which are concatenated to the end of the
 *  bstring b.  The stream read is terminated by the passed in terminator
 *  parameter.
 *
 *  If getcPtr returns with a negative number, or the terminator character
 *  (which is appended) is read, then the stream reading is halted and the
 *  function returns with a partial result concatentated to b.  If there is
 *  an empty partial result, 1 is returned.  If no characters are read, or
 *  there is some other detectable error, BSTR_ERR is returned.
 */
int bgetsa(bstring* b, bNgetc getcPtr, void * parm, char terminator) {
	int c, d, e;

	BSTR_TEST_ASSERT(b && bisvalid(b));
	ASSERT(b && bisvalid(b));
	BSTR_TEST_ASSERT(getcPtr != NULL);
	ASSERT(getcPtr != NULL);

	d = b->slen;
	e = bmlen(b) - 2;

	while ((c = getcPtr(parm)) >= 0) {
		if (d > e) {
			b->slen = d;
			int allocResult = balloc(b, d + 2);
			ASSERT(allocResult == BSTR_OK);
			e = bmlen(b) - 2;
		}
		b->data[d] = (unsigned char)c;
		d++;
		if (c == terminator) break;
	}

	if (b->data)
		writeNull(b, d);
	b->slen = d;

	return d == 0 && c < 0;
}

/*  bstring bread (bNread readPtr, void * parm)
 *
 *  Use a finite buffer fread-like function readPtr to create a bstring
 *  filled with the entire contents of file-like source data in a roughly
 *  efficient way.
 */
bstring* bread(bNread readPtr, void * parm) {
	bstring* buff;

	if (0 > breada(buff = bfromcstr(""), readPtr, parm)) {
		bdestroy(buff);
		return NULL;
	}
	return buff;
}

/*  bstring bgets (bNgetc getcPtr, void * parm, char terminator)
 *
 *  Use an fgetc-like single character stream reading function (getcPtr) to
 *  obtain a sequence of characters which are concatenated into a bstring.
 *  The stream read is terminated by the passed in terminator function.
 *
 *  If getcPtr returns with a negative number, or the terminator character
 *  (which is appended) is read, then the stream reading is halted and the
 *  result obtained thus far is returned.  If no characters are read, or
 *  there is some other detectable error, NULL is returned.
 */
bstring* bgets(bNgetc getcPtr, void * parm, char terminator) {
	bstring* buff;

	if (0 > bgetsa(buff = bfromcstr(""), getcPtr, parm, terminator) ||
		0 >= buff->slen) {
		bdestroy(buff);
		buff = NULL;
	}
	return buff;
}

struct bStream {
	bstring* buff;		/* Buffer for over-reads */
	void * parm;		/* The stream handle for core stream */
	bNread readFnPtr;	/* fread compatible fnptr for core stream */
	int isEOF;			/* track file's EOF state */
	int maxBuffSz;
};

/*  struct bStream * bsopen (bNread readPtr, void * parm)
 *
 *  Wrap a given open stream (described by a fread compatible function
 *  pointer and stream handle) into an open bStream suitable for the bstring
 *  library streaming functions.
 */
struct bStream * bsopen (bNread readPtr, void * parm) {
struct bStream * s;

	if (readPtr == NULL) return NULL;
	s = (struct bStream *) tf_malloc (sizeof (struct bStream));
	if (s == NULL) return NULL;
	s->parm = parm;
	s->buff = bfromcstr ("");
	s->readFnPtr = readPtr;
	s->maxBuffSz = BS_BUFF_SZ;
	s->isEOF = 0;
	return s;
}

/*  int bsbufflength (struct bStream * s, int sz)
 *
 *  Set the length of the buffer used by the bStream.  If sz is zero, the
 *  length is not set.  This function returns with the previous length.
 */
int bsbufflength (struct bStream * s, int sz) {
int oldSz;
	if (s == NULL || sz < 0) return BSTR_ERR;
	oldSz = s->maxBuffSz;
	if (sz > 0) s->maxBuffSz = sz;
	return oldSz;
}

int bseof (const struct bStream * s) {
	if (s == NULL || s->readFnPtr == NULL) return BSTR_ERR;
	return s->isEOF && (s->buff->slen == 0);
}

/*  void * bsclose (struct bStream * s)
 *
 *  Close the bStream, and return the handle to the stream that was originally
 *  used to open the given stream.
 */
void * bsclose (struct bStream * s) {
void * parm;
	if (s == NULL) return NULL;
	s->readFnPtr = NULL;
	if (s->buff) bdestroy (s->buff);
	s->buff = NULL;
	parm = s->parm;
	s->parm = NULL;
	s->isEOF = 1;
	tf_free (s);
	return parm;
}

/*  int bsreadlna (bstring r, struct bStream * s, char terminator)
 *
 *  Read a bstring terminated by the terminator character or the end of the
 *  stream from the bStream (s) and return it into the parameter r.  This
 *  function may read additional characters from the core stream that are not
 *  returned, but will be retained for subsequent read operations.
 */
int bsreadlna (bstring* r, struct bStream * s, char terminator) {
int i, l, ret, rlo;
char * b;
struct tagbstring x;

	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0 ||
	    r->slen < 0 || r->mlen < r->slen) return BSTR_ERR;
	l = s->buff->slen;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	b = (char *) s->buff->data;
	x.data = (unsigned char *) b;

	/* First check if the current buffer holds the terminator */
	b[l] = terminator; /* Set sentinel */
	for (i=0; b[i] != terminator; i++) ;
	if (i < l) {
		x.slen = i + 1;
		ret = bconcat (r, &x);
		s->buff->slen = l;
		if (BSTR_OK == ret) bdelete (s->buff, 0, i + 1);
		return BSTR_OK;
	}

	rlo = r->slen;

	/* If not then just concatenate the entire buffer to the output */
	x.slen = l;
	if (BSTR_OK != bconcat (r, &x)) return BSTR_ERR;

	/* Perform direct in-place reads into the destination to allow for
	   the minimum of data-copies */
	for (;;) {
		if (BSTR_OK != balloc (r, r->slen + s->maxBuffSz + 1))
		    return BSTR_ERR;
		b = (char *) (r->data + r->slen);
		l = (int) s->readFnPtr (b, 1, s->maxBuffSz, s->parm);
		if (l <= 0) {
			r->data[r->slen] = (unsigned char) '\0';
			s->buff->slen = 0;
			s->isEOF = 1;
			/* If nothing was read return with an error message */
			return BSTR_ERR & -(r->slen == rlo);
		}
		b[l] = terminator; /* Set sentinel */
		for (i=0; b[i] != terminator; i++) ;
		if (i < l) break;
		r->slen += l;
	}

	/* Terminator found, push over-read back to buffer */
	i++;
	r->slen += i;
	s->buff->slen = l - i;
	memcpy (s->buff->data, b + i, l - i);
	r->data[r->slen] = (unsigned char) '\0';
	return BSTR_OK;
}

/*  int bsreadlnsa (bstring r, struct bStream * s, bstring term)
 *
 *  Read a bstring terminated by any character in the term string or the end
 *  of the stream from the bStream (s) and return it into the parameter r.
 *  This function may read additional characters from the core stream that
 *  are not returned, but will be retained for subsequent read operations.
 */
int bsreadlnsa (bstring* r, struct bStream * s, const bstring* term) {
int i, l, ret, rlo;
unsigned char * b;
struct tagbstring x;
struct charField cf;

	if (s == NULL || s->buff == NULL || r == NULL || term == NULL ||
	    term->data == NULL || r->mlen <= 0 || r->slen < 0 ||
	    r->mlen < r->slen) return BSTR_ERR;
	if (term->slen == 1) return bsreadlna (r, s, term->data[0]);
	if (term->slen < 1 || buildCharField (&cf, term)) return BSTR_ERR;

	l = s->buff->slen;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	b = (unsigned char *) s->buff->data;
	x.data = b;

	/* First check if the current buffer holds the terminator */
	b[l] = term->data[0]; /* Set sentinel */
	for (i=0; !testInCharField (&cf, b[i]); i++) ;
	if (i < l) {
		x.slen = i + 1;
		ret = bconcat (r, &x);
		s->buff->slen = l;
		if (BSTR_OK == ret) bdelete (s->buff, 0, i + 1);
		return BSTR_OK;
	}

	rlo = r->slen;

	/* If not then just concatenate the entire buffer to the output */
	x.slen = l;
	if (BSTR_OK != bconcat (r, &x)) return BSTR_ERR;

	/* Perform direct in-place reads into the destination to allow for
	   the minimum of data-copies */
	for (;;) {
		if (BSTR_OK != balloc (r, r->slen + s->maxBuffSz + 1))
		    return BSTR_ERR;
		b = (unsigned char *) (r->data + r->slen);
		l = (int) s->readFnPtr (b, 1, s->maxBuffSz, s->parm);
		if (l <= 0) {
			r->data[r->slen] = (unsigned char) '\0';
			s->buff->slen = 0;
			s->isEOF = 1;
			/* If nothing was read return with an error message */
			return BSTR_ERR & -(r->slen == rlo);
		}

		b[l] = term->data[0]; /* Set sentinel */
		for (i=0; !testInCharField (&cf, b[i]); i++) ;
		if (i < l) break;
		r->slen += l;
	}

	/* Terminator found, push over-read back to buffer */
	i++;
	r->slen += i;
	s->buff->slen = l - i;
	memcpy (s->buff->data, b + i, l - i);
	r->data[r->slen] = (unsigned char) '\0';
	return BSTR_OK;
}

/*  int bsreada (bstring r, struct bStream * s, int n)
 *
 *  Read a bstring of length n (or, if it is fewer, as many bytes as is
 *  remaining) from the bStream.  This function may read additional
 *  characters from the core stream that are not returned, but will be
 *  retained for subsequent read operations.  This function will not read
 *  additional characters from the core stream beyond virtual stream pointer.
 */
int bsreada (bstring* r, struct bStream * s, int n) {
int l, ret, orslen;
char * b;
struct tagbstring x;

	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0
	 || r->slen < 0 || r->mlen < r->slen || n <= 0) return BSTR_ERR;

	if (n > INT_MAX - r->slen) return BSTR_ERR;
	n += r->slen;

	l = s->buff->slen;

	orslen = r->slen;

	if (0 == l) {
		if (s->isEOF) return BSTR_ERR;
		if (r->mlen > n) {
			l = (int) s->readFnPtr (r->data + r->slen, 1, n - r->slen,
			                        s->parm);
			if (0 >= l || l > n - r->slen) {
				s->isEOF = 1;
				return BSTR_ERR;
			}
			r->slen += l;
			r->data[r->slen] = (unsigned char) '\0';
			return 0;
		}
	}

	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	b = (char *) s->buff->data;
	x.data = (unsigned char *) b;

	do {
		if (l + r->slen >= n) {
			x.slen = n - r->slen;
			ret = bconcat (r, &x);
			s->buff->slen = l;
			if (BSTR_OK == ret) bdelete (s->buff, 0, x.slen);
			return BSTR_ERR & -(r->slen == orslen);
		}

		x.slen = l;
		if (BSTR_OK != bconcat (r, &x)) break;

		l = n - r->slen;
		if (l > s->maxBuffSz) l = s->maxBuffSz;

		l = (int) s->readFnPtr (b, 1, l, s->parm);

	} while (l > 0);
	if (l < 0) l = 0;
	if (l == 0) s->isEOF = 1;
	s->buff->slen = l;
	return BSTR_ERR & -(r->slen == orslen);
}

/*  int bsreadln (bstring r, struct bStream * s, char terminator)
 *
 *  Read a bstring terminated by the terminator character or the end of the
 *  stream from the bStream (s) and return it into the parameter r.  This
 *  function may read additional characters from the core stream that are not
 *  returned, but will be retained for subsequent read operations.
 */
int bsreadln (bstring* r, struct bStream * s, char terminator) {
	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0)
		return BSTR_ERR;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	r->slen = 0;
	return bsreadlna (r, s, terminator);
}

/*  int bsreadlns (bstring r, struct bStream * s, bstring term)
 *
 *  Read a bstring terminated by any character in the term string or the end
 *  of the stream from the bStream (s) and return it into the parameter r.
 *  This function may read additional characters from the core stream that
 *  are not returned, but will be retained for subsequent read operations.
 */
int bsreadlns (bstring* r, struct bStream * s, const bstring* term) {
	if (s == NULL || s->buff == NULL || r == NULL || term == NULL
	 || term->data == NULL || r->mlen <= 0) return BSTR_ERR;
	if (term->slen == 1) return bsreadln (r, s, term->data[0]);
	if (term->slen < 1) return BSTR_ERR;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	r->slen = 0;
	return bsreadlnsa (r, s, term);
}

/*  int bsread (bstring r, struct bStream * s, int n)
 *
 *  Read a bstring of length n (or, if it is fewer, as many bytes as is
 *  remaining) from the bStream.  This function may read additional
 *  characters from the core stream that are not returned, but will be
 *  retained for subsequent read operations.  This function will not read
 *  additional characters from the core stream beyond virtual stream pointer.
 */
int bsread (bstring* r, struct bStream * s, int n) {
	if (s == NULL || s->buff == NULL || r == NULL || r->mlen <= 0
	 || n <= 0) return BSTR_ERR;
	if (BSTR_OK != balloc (s->buff, s->maxBuffSz + 1)) return BSTR_ERR;
	r->slen = 0;
	return bsreada (r, s, n);
}

/*  int bsunread (struct bStream * s, const bstring* b)
 *
 *  Insert a bstring into the bStream at the current position.  These
 *  characters will be read prior to those that actually come from the core
 *  stream.
 */
int bsunread (struct bStream * s, const bstring* b) {
	if (s == NULL || s->buff == NULL) return BSTR_ERR;
	return binsert (s->buff, 0, b, (unsigned char) '?');
}

/*  int bspeek (bstring r, const struct bStream * s)
 *
 *  Return the currently buffered characters from the bStream that will be
 *  read prior to reads from the core stream.
 */
int bspeek (bstring* r, const struct bStream * s) {
	if (s == NULL || s->buff == NULL) return BSTR_ERR;
	return bassign (r, s->buff);
}

#define BSSSC_BUFF_LEN (256)

/*  int bssplitscb (struct bStream * s, const bstring* splitStr,
 *                  int (* cb) (void * parm, int ofs, const bstring* entry),
 *                  void * parm)
 *
 *  Iterate the set of disjoint sequential substrings read from a stream
 *  divided by any of the characters in splitStr.  An empty splitStr causes
 *  the whole stream to be iterated once.
 *
 *  Note: At the point of calling the cb function, the bStream pointer is
 *  pointed exactly at the position right after having read the split
 *  character.  The cb function can act on the stream by causing the bStream
 *  pointer to move, and bssplitscb will continue by starting the next split
 *  at the position of the pointer after the return from cb.
 *
 *  However, if the cb causes the bStream s to be destroyed then the cb must
 *  return with a negative value, otherwise bssplitscb will continue in an
 *  undefined manner.
 */
int bssplitscb(struct bStream * s, const bstring* splitStr,
	int(*cb) (void * parm, int ofs, const bstring* entry), void * parm) {
	struct charField chrs;
	bstring* buff;
	int i, p, ret;

	if (cb == NULL || s == NULL || s->readFnPtr == NULL ||
		splitStr == NULL || splitStr->slen < 0) return BSTR_ERR;

	if (NULL == (buff = bfromcstr(""))) return BSTR_ERR;

	if (splitStr->slen == 0) {
		while (bsreada(buff, s, BSSSC_BUFF_LEN) >= 0);
		if ((ret = cb(parm, 0, buff)) > 0)
			ret = 0;
	}
	else {
		buildCharField(&chrs, splitStr);
		ret = p = i = 0;
		for (;;) {
			if (i >= buff->slen) {
				bsreada(buff, s, BSSSC_BUFF_LEN);
				if (i >= buff->slen) {
					if (0 < (ret = cb(parm, p, buff))) ret = 0;
					break;
				}
			}
			if (testInCharField(&chrs, buff->data[i])) {
				struct tagbstring t;
				unsigned char c;

				blk2tbstr(t, buff->data + i + 1, buff->slen - (i + 1));
				if ((ret = bsunread(s, &t)) < 0) break;
				buff->slen = i;
				c = buff->data[i];
				buff->data[i] = (unsigned char) '\0';
				if ((ret = cb(parm, p, buff)) < 0) break;
				buff->data[i] = c;
				buff->slen = 0;
				p += i + 1;
				i = -1;
			}
			i++;
		}
	}

	bdestroy(buff);
	return ret;
}

/*  int bssplitstrcb (struct bStream * s, const bstring* splitStr,
 *                    int (* cb) (void * parm, int ofs, const bstring* entry),
 *                    void * parm)
 *
 *  Iterate the set of disjoint sequential substrings read from a stream
 *  divided by the entire substring splitStr.  An empty splitStr causes
 *  each character of the stream to be iterated.
 *
 *  Note: At the point of calling the cb function, the bStream pointer is
 *  pointed exactly at the position right after having read the split
 *  character.  The cb function can act on the stream by causing the bStream
 *  pointer to move, and bssplitscb will continue by starting the next split
 *  at the position of the pointer after the return from cb.
 *
 *  However, if the cb causes the bStream s to be destroyed then the cb must
 *  return with a negative value, otherwise bssplitscb will continue in an
 *  undefined manner.
 */
int bssplitstrcb(struct bStream * s, const bstring* splitStr,
	int(*cb) (void * parm, int ofs, const bstring* entry), void * parm) {
	bstring* buff;
	int i, p, ret;

	if (cb == NULL || s == NULL || s->readFnPtr == NULL
		|| splitStr == NULL || splitStr->slen < 0) return BSTR_ERR;

	if (splitStr->slen == 1) return bssplitscb(s, splitStr, cb, parm);

	if (NULL == (buff = bfromcstr(""))) return BSTR_ERR;

	if (splitStr->slen == 0) {
		for (i = 0; bsreada(buff, s, BSSSC_BUFF_LEN) >= 0; i++) {
			if ((ret = cb(parm, 0, buff)) < 0) {
				bdestroy(buff);
				return ret;
			}
			buff->slen = 0;
		}
		return BSTR_OK;
	}
	else {
		ret = p = i = 0;
		for (i = p = 0;;) {
			if ((ret = binstr(buff, 0, splitStr)) >= 0) {
				struct tagbstring t;
				blk2tbstr(t, buff->data, ret);
				i = ret + splitStr->slen;
				if ((ret = cb(parm, p, &t)) < 0) break;
				p += i;
				bdelete(buff, 0, i);
			}
			else {
				bsreada(buff, s, BSSSC_BUFF_LEN);
				if (bseof(s)) {
					if ((ret = cb(parm, p, buff)) > 0) ret = 0;
					break;
				}
			}
		}
	}

	bdestroy(buff);
	return ret;
}

/*  int bstrListCreate (void)
 *
 *  Create a bstrList.
 */
struct bstrList * bstrListCreate(void) {
	struct bstrList * sl =
		(struct bstrList *) tf_malloc(sizeof(struct bstrList));
	if (sl) {
		sl->entry = (bstring* *)tf_malloc(1 * sizeof(bstring*));
		if (!sl->entry) {
			tf_free(sl);
			sl = NULL;
		}
		else {
			sl->qty = 0;
			sl->mlen = 1;
		}
	}
	return sl;
}

/*  int bstrListDestroy (struct bstrList * sl)
 *
 *  Destroy a bstrList that has been created by bsplit, bsplits or
 *  bstrListCreate.
 */
int bstrListDestroy(struct bstrList * sl) {
	int i;
	if (sl == NULL || sl->qty < 0) return BSTR_ERR;
	for (i = 0; i < sl->qty; i++) {
		if (sl->entry[i]) {
			bdestroy(sl->entry[i]);
			sl->entry[i] = NULL;
		}
	}
	sl->qty = -1;
	sl->mlen = -1;
	tf_free(sl->entry);
	sl->entry = NULL;
	tf_free(sl);
	return BSTR_OK;
}

/*  int bstrListAlloc (struct bstrList * sl, int msz)
 *
 *  Ensure that there is memory for at least msz number of entries for the
 *  list.
 */
int bstrListAlloc(struct bstrList * sl, int msz) {
	bstring* * l;
	int smsz;
	size_t nsz;
	if (!sl || msz <= 0 || !sl->entry || sl->qty < 0 || sl->mlen <= 0 ||
		sl->qty > sl->mlen) return BSTR_ERR;
	if (sl->mlen >= msz) return BSTR_OK;
	smsz = snapUpSize(msz);
	nsz = ((size_t)smsz) * sizeof(bstring*);
	if (nsz < (size_t)smsz) return BSTR_ERR;
	l = (bstring* *)tf_realloc(sl->entry, nsz);
	if (!l) {
		smsz = msz;
		nsz = ((size_t)smsz) * sizeof(bstring*);
		l = (bstring* *)tf_realloc(sl->entry, nsz);
		if (!l) return BSTR_ERR;
	}
	sl->mlen = smsz;
	sl->entry = l;
	return BSTR_OK;
}

/*  int bstrListAllocMin (struct bstrList * sl, int msz)
 *
 *  Try to allocate the minimum amount of memory for the list to include at
 *  least msz entries or sl->qty whichever is greater.
 */
int bstrListAllocMin(struct bstrList * sl, int msz) {
	bstring* * l;
	size_t nsz;
	if (!sl || msz <= 0 || !sl->entry || sl->qty < 0 || sl->mlen <= 0 ||
		sl->qty > sl->mlen) return BSTR_ERR;
	if (msz < sl->qty) msz = sl->qty;
	if (sl->mlen == msz) return BSTR_OK;
	nsz = ((size_t)msz) * sizeof(bstring*);
	if (nsz < (size_t)msz) return BSTR_ERR;
	l = (bstring* *)tf_realloc(sl->entry, nsz);
	if (!l) return BSTR_ERR;
	sl->mlen = msz;
	sl->entry = l;
	return BSTR_OK;
}

#if defined (__TURBOC__) && !defined (__BORLANDC__)
# ifndef BSTRLIB_NOVSNP
#  define BSTRLIB_NOVSNP
# endif
#endif

/* Give WATCOM C/C++, MSVC some latitude for their non-support of vsnprintf */
#if defined(__WATCOMC__) || defined(_MSC_VER)
#define exvsnprintf(r,b,n,f,a) {r = _vsnprintf (b,n,f,a);}
#else
#ifdef BSTRLIB_NOVSNP
/* This is just a hack.  If you are using a system without a vsnprintf, it is
   not recommended that bformat be used at all. */
#define exvsnprintf(r,b,n,f,a) {vsprintf (b,f,a); r = -1;}
#define START_VSNBUFF (256)
#else

#if defined(__GNUC__) && !defined(__APPLE__)
/* Something is making gcc complain about this prototype not being here, so
   I've just gone ahead and put it in. */
extern int vsnprintf(char *buf, size_t count, const char *format, va_list arg);
#endif

#define exvsnprintf(r,b,n,f,a) {r = vsnprintf (b,n,f,a);}
#endif
#endif

#if !defined (BSTRLIB_NOVSNP)

#ifndef START_VSNBUFF
#define START_VSNBUFF (16)
#endif

/* On IRIX vsnprintf returns n-1 when the operation would overflow the target
   buffer, WATCOM and MSVC both return -1, while C99 requires that the
   returned value be exactly what the length would be if the buffer would be
   large enough.  This leads to the idea that if the return value is larger
   than n, then changing n to the return value will reduce the number of
   iterations required. */

   /*  int bformata (bstring b, const char * fmt, ...)
	*
	*  After the first parameter, it takes the same parameters as printf (), but
	*  rather than outputting results to stdio, it appends the results to
	*  a bstring which contains what would have been output. Note that if there
	*  is an early generation of a '\0' character, the bstring will be truncated
	*  to this end point.
	*/
int bformata(bstring* b, const char * fmt, ...) {
	va_list arglist;
	bstring* buff;
	int n, r;

	if (b == NULL || fmt == NULL || b->data == NULL || b->mlen <= 0
		|| b->slen < 0 || b->slen > b->mlen) return BSTR_ERR;

	/* Since the length is not determinable beforehand, a search is
	   performed using the truncating "vsnprintf" call (to avoid buffer
	   overflows) on increasing potential sizes for the output result. */

	if ((n = (int)(2 * strlen(fmt))) < START_VSNBUFF) n = START_VSNBUFF;
	if (NULL == (buff = bfromcstralloc(n + 2, ""))) {
		n = 1;
		if (NULL == (buff = bfromcstralloc(n + 2, ""))) return BSTR_ERR;
	}

	for (;;) {
		va_start(arglist, fmt);
		exvsnprintf(r, (char *)buff->data, n + 1, fmt, arglist);
		va_end(arglist);

		buff->data[n] = (unsigned char) '\0';
		buff->slen = (int)(strlen)((char *)buff->data);

		if (buff->slen < n) break;

		if (r > n) n = r; else n += n;

		if (BSTR_OK != balloc(buff, n + 2)) {
			bdestroy(buff);
			return BSTR_ERR;
		}
	}

	r = bconcat(b, buff);
	bdestroy(buff);
	return r;
}

/*  int bassignformat (bstring b, const char * fmt, ...)
 *
 *  After the first parameter, it takes the same parameters as printf (), but
 *  rather than outputting results to stdio, it outputs the results to
 *  the bstring parameter b. Note that if there is an early generation of a
 *  '\0' character, the bstring will be truncated to this end point.
 */
int bassignformat(bstring* b, const char * fmt, ...) {
	va_list arglist;
	bstring* buff;
	int n, r;

	if (b == NULL || fmt == NULL || b->data == NULL || b->mlen <= 0
		|| b->slen < 0 || b->slen > b->mlen) return BSTR_ERR;

	/* Since the length is not determinable beforehand, a search is
	   performed using the truncating "vsnprintf" call (to avoid buffer
	   overflows) on increasing potential sizes for the output result. */

	if ((n = (int)(2 * strlen(fmt))) < START_VSNBUFF) n = START_VSNBUFF;
	if (NULL == (buff = bfromcstralloc(n + 2, ""))) {
		n = 1;
		if (NULL == (buff = bfromcstralloc(n + 2, ""))) return BSTR_ERR;
	}

	for (;;) {
		va_start(arglist, fmt);
		exvsnprintf(r, (char *)buff->data, n + 1, fmt, arglist);
		va_end(arglist);

		buff->data[n] = (unsigned char) '\0';
		buff->slen = (int)(strlen)((char *)buff->data);

		if (buff->slen < n) break;

		if (r > n) n = r; else n += n;

		if (BSTR_OK != balloc(buff, n + 2)) {
			bdestroy(buff);
			return BSTR_ERR;
		}
	}

	r = bassign(b, buff);
	bdestroy(buff);
	return r;
}

/*  bstring bformat (const char * fmt, ...)
 *
 *  Takes the same parameters as printf (), but rather than outputting results
 *  to stdio, it forms a bstring which contains what would have been output.
 *  Note that if there is an early generation of a '\0' character, the
 *  bstring will be truncated to this end point.
 */
bstring* bformat(const char * fmt, ...) {
	va_list arglist;
	bstring* buff;
	int n, r;

	if (fmt == NULL) return NULL;

	/* Since the length is not determinable beforehand, a search is
	   performed using the truncating "vsnprintf" call (to avoid buffer
	   overflows) on increasing potential sizes for the output result. */

	if ((n = (int)(2 * strlen(fmt))) < START_VSNBUFF) n = START_VSNBUFF;
	if (NULL == (buff = bfromcstralloc(n + 2, ""))) {
		n = 1;
		if (NULL == (buff = bfromcstralloc(n + 2, ""))) return NULL;
	}

	for (;;) {
		va_start(arglist, fmt);
		exvsnprintf(r, (char *)buff->data, n + 1, fmt, arglist);
		va_end(arglist);

		buff->data[n] = (unsigned char) '\0';
		buff->slen = (int)(strlen)((char *)buff->data);

		if (buff->slen < n) break;

		if (r > n) n = r; else n += n;

		if (BSTR_OK != balloc(buff, n + 2)) {
			bdestroy(buff);
			return NULL;
		}
	}

	return buff;
}

/*  int bvcformata (bstring b, int count, const char * fmt, va_list arglist)
 *
 *  The bvcformata function formats data under control of the format control
 *  string fmt and attempts to append the result to b.  The fmt parameter is
 *  the same as that of the printf function.  The variable argument list is
 *  replaced with arglist, which has been initialized by the va_start macro.
 *  The size of the output is upper bounded by count.  If the required output
 *  exceeds count, the string b is not augmented with any contents and a value
 *  below BSTR_ERR is returned.  If a value below -count is returned then it
 *  is recommended that the negative of this value be used as an update to the
 *  count in a subsequent pass.  On other errors, such as running out of
 *  memory, parameter errors or numeric wrap around BSTR_ERR is returned.
 *  BSTR_OK is returned when the output is successfully generated and
 *  appended to b.
 *
 *  Note: There is no sanity checking of arglist, and this function is
 *  destructive of the contents of b from the b->slen point onward.  If there
 *  is an early generation of a '\0' character, the bstring will be truncated
 *  to this end point.
 */
int bvcformata(bstring* b, int count, const char * fmt, va_list arg) {
	int n, r, l;

	if (b == NULL || fmt == NULL || count <= 0 || b->data == NULL
		|| b->mlen <= 0 || b->slen < 0 || b->slen > b->mlen) return BSTR_ERR;

	if (count > (n = b->slen + count) + 2) return BSTR_ERR;
	if (BSTR_OK != balloc(b, n + 2)) return BSTR_ERR;

	exvsnprintf(r, (char *)b->data + b->slen, count + 2, fmt, arg);
	writeNull(b, b->slen + count + 2);

	/* Did the operation complete successfully within bounds? */

	if (n >= (l = b->slen + (int)(strlen)((char *)b->data + b->slen))) {
		b->slen = l;
		return BSTR_OK;
	}

	/* Abort, since the buffer was not large enough.  The return value
	   tries to help set what the retry length should be. */

	writeNull(b, b->slen);
	if (r > count + 1) {
		l = r;
	}
	else {
		if (count > INT_MAX / 2)
			l = INT_MAX;
		else
			l = count + count;
	}
	n = -l;
	if (n > BSTR_ERR - 1) n = BSTR_ERR - 1;
	return n;
}

#endif

#endif


#endif
