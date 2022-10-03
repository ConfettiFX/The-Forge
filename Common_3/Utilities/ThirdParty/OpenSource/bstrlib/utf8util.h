/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002-2015, and is covered by the BSD open source
 * license and the GPL. Refer to the accompanying documentation for details
 * on usage and license.
 */

/*
 * utf8util.h
 *
 * This file defines the interface to the utf8 utility functions.
 */

#ifndef UTF8_UNICODE_UTILITIES
#define UTF8_UNICODE_UTILITIES

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#if INT_MAX >= 0x7fffffffUL
typedef int 			cpUcs4;
#elif LONG_MAX >= 0x7fffffffUL
typedef long			cpUcs4;
#else
#error This compiler is not supported
#endif

#if UINT_MAX == 0xFFFF
typedef unsigned int	cpUcs2;
#elif USHRT_MAX == 0xFFFF
typedef unsigned short	cpUcs2;
#elif UCHAR_MAX == 0xFFFF
typedef unsigned char	cpUcs2;
#else
#error This compiler is not supported
#endif

#define isLegalUnicodeCodePoint(v) ((((v) < 0xD800L) || ((v) > 0xDFFFL)) && (((unsigned long)(v)) <= 0x0010FFFFL) && (((v)|0x1F0001) != 0x1FFFFFL))

struct utf8Iterator {
	unsigned char*	data;
	int           	slen;
	int           	start, next;
	int           	error;
};

#define utf8IteratorNoMore(it) (!(it) || (it)->next >= (it)->slen)

extern void utf8IteratorInit (struct utf8Iterator* iter, unsigned char* data, int slen);
extern void utf8IteratorUninit (struct utf8Iterator* iter);
extern cpUcs4 utf8IteratorGetNextCodePoint (struct utf8Iterator* iter, cpUcs4 errCh);
extern cpUcs4 utf8IteratorGetCurrCodePoint (struct utf8Iterator* iter, cpUcs4 errCh);
extern int utf8ScanBackwardsForCodePoint (unsigned char* msg, int len, int pos, cpUcs4* out);

#ifdef __cplusplus
}
#endif

#endif /* UTF8_UNICODE_UTILITIES */
