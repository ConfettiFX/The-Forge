/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002-2015, and is covered by the BSD open source
 * license and the GPL. Refer to the accompanying documentation for details
 * on usage and license.
 */

/*
 * The Forge: Please refer to TFREADME.md for all changes and updated documentation
*/

/*
 * bstrlib.h
 *
 * This file is the interface for the core bstring functions.
 */

#ifndef BSTRLIB_INCLUDE
#define BSTRLIB_INCLUDE

#include "../../../../Application/Config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdbool.h>

#if !defined (BSTRLIB_VSNP_OK) && !defined (BSTRLIB_NOVSNP)
# if defined (__TURBOC__) && !defined (__BORLANDC__)
#  define BSTRLIB_NOVSNP
# endif
#endif

// Assert error is only for tests
#define BSTR_ASSERT_ERR (INT_MIN)
#define BSTR_ERR (-1)
#define BSTR_CMP_EXTRA_NULL ((int)UCHAR_MAX + 1)
#define BSTR_OK (0)
#define BSTR_BS_BUFF_LENGTH_GET (0)

/* Version */
#define BSTR_VER_MAJOR  1
#define BSTR_VER_MINOR  0
#define BSTR_VER_UPDATE 0

#define BSTR_DYNAMIC_BIT (~INT_MAX)

#ifdef ENABLE_MEMORY_TRACKING
#define BSTR_STRINGIFY(x) #x
#define BSTR_DECLARE_FN(ret, name, ...) ret name##Impl(const char* FILE_NAME, int FILE_LINE, const char* FUNCTION_NAME, const char* PARENT_FUNCTION_NAME OPT_COMMA_VA_ARGS(__VA_ARGS__))
#define BSTR_CALL(fn, ...) fn##Impl(__FILE__, __LINE__, __FUNCTION__, BSTR_STRINGIFY(fn) OPT_COMMA_VA_ARGS(__VA_ARGS__))
#else
#define BSTR_DECLARE_FN(ret, name, ...) ret name##Impl (__VA_ARGS__)
#define BSTR_CALL(fn, ...) fn##Impl(__VA_ARGS__)
#endif

typedef struct bstring {
	int mlen;
	int slen;
	unsigned char * data;
}bstring;

/* Accessor macros */
#define blengthe(b, e)      (((b) == (void *)0 || (b)->slen < 0) ? (int)(e) : ((b)->slen))
#define blength(b)          (blengthe ((b), 0))
#define bownsdata(b)       (((b)->mlen & BSTR_DYNAMIC_BIT) != 0)
/* use bcapacity to ensure that b is not null */
#define bmlen(b)            (((b)->mlen) & ~BSTR_DYNAMIC_BIT)
#define bcapacitye(b, e)    ( ((b) == NULL) ? (int)(e) : bmlen((b)) )
#define bcapacity(b)        (bcapacitye((b),0))
#define bdataofse(b, o, e)  (((b) == (void *)0 || (b)->data == (void*)0) ? (char *)(e) : ((char *)(b)->data) + (o))
#define bdataofs(b, o)      (bdataofse ((b), (o), (void *)0))
#define bdatae(b, e)        (bdataofse ((b), 0, (e)))
#define bdata(b)            (bdataofs ((b), 0))
#define bchare(b, p, e)     ((((unsigned)(p)) < (unsigned)blength((b))) ? ((b)->data[(p)]) : (e))
#define bchar(b, p)         bchare ((b), (p), '\0')

FORGE_API extern char gEmptyStringBuffer[1];

#define bconstisvalid(b)    ((b != NULL) && ((b)->slen >= 0) && ((b)->data))
#define bisvalid(b)         ( bconstisvalid((b)) && bmlen((b)) > 0 && bmlen((b)) >= (b)->slen )
#define biscstr(b)			( bconstisvalid((b)) && (b)->data[(b)->slen] == '\0' )


/* Dynamic string initialization functions */

/*
 * Creates string allocated on the heap if provided string is not empty.
 * If provided string is empty underlying buffer will point to 
 * global shared empty string buffer
 */
FORGE_API extern BSTR_DECLARE_FN(bstring, bdynfromcstr, const char* pCStr);
#define bdynfromcstr(pCStr) BSTR_CALL(bdynfromcstr, pCStr)
/*
 * Creates string allocated on the heap if provided string is not empty or minCapacity > 1.
 * If provided string is empty and minCapacity <= 1 underlying buffer will point to
 * global shared empty string buffer
 */
FORGE_API extern BSTR_DECLARE_FN(bstring, bdynallocfromcstr, const char* str, int minCapacity);
#define bdynallocfromcstr(str, minCapacity) BSTR_CALL(bdynallocfromcstr, str, minCapacity)
/*
 * Creates string allocated on the heap if len > 0 or minCapacity > 1.
 * If len == 0 and minCapacity <= 1 underlying buffer will point to
 * global shared empty string buffer
 */
FORGE_API extern BSTR_DECLARE_FN(bstring, bdynfromblk, const void* blk, int len, int minCapacity);
#define bdynfromblk(blk, len, minCapacity) BSTR_CALL(bdynfromblk, blk, len, minCapacity)
/*
 * Creates string allocated on the heap if b1->slen > 0 or minCapacity > 1.
 * If b1->slen == 0 and minCapacity <= 1 underlying buffer will point to
 * global shared empty string buffer
 */
FORGE_API extern BSTR_DECLARE_FN(bstring, bdynfromstr, const bstring* b1, int minCapacity);
#define bdynfromstr(b1, minCapacity) BSTR_CALL(bdynfromstr, b1, minCapacity)

/* Initialization of bstring from array */
#ifdef __cplusplus
/* Initializes internal buffer to s */
#define bfromarr(arr) {sizeof(arr)/sizeof((arr)[0]), (int)strlen((const char*)&(arr)[0]), (unsigned char*)(&(arr)[0])}
/* Initializes internal buffer to s, ignores initial content of arr */
#define bemptyfromarr(arr) { sizeof(arr)/sizeof((arr)[0]), ((arr)[0] = '\0'), (unsigned char*)(&(arr)[0])}
#else
/* Initializes internal buffer to arr */
#define bfromarr(arr) (bstring){sizeof(arr)/sizeof(arr[0]), strlen(arr), (unsigned char*)(arr)}
/* Initializes internal buffer to arr, ignores initial content of arr */
#define bemptyfromarr(arr) (bstring){ sizeof(arr)/sizeof(arr[0]), ((arr)[0] = '\0'), (unsigned char*)(arr)}
#endif

#ifdef __cplusplus
/* Initializes internal buffer to shared empty string */
#define bempty() {1,0,(unsigned char*)(&gEmptyStringBuffer[0])}
#else
/* Initializes internal buffer to shared empty string */
#define bempty() (bstring){1,0,(unsigned char*)(&gEmptyStringBuffer[0])}
#endif


/* Ensures that underlying buffer is a c string */
FORGE_API extern BSTR_DECLARE_FN(int, bmakecstr, bstring* b);
#define bmakecstr(b) BSTR_CALL(bmakecstr, b)

FORGE_API extern BSTR_DECLARE_FN(int, bassign, bstring* a, const bstring* b);
#define bassign(a, b) BSTR_CALL(bassign, a, b)
FORGE_API extern BSTR_DECLARE_FN(int, bassignmidstr, bstring* a, const bstring* b, int left, int len);
#define bassignmidstr(a, b, left, len) BSTR_CALL(bassignmidstr , a, b, left, len)
FORGE_API extern BSTR_DECLARE_FN(int, bassigncstr, bstring* a, const char * str);
#define bassigncstr(a, str) BSTR_CALL(bassigncstr, a, str)
FORGE_API extern BSTR_DECLARE_FN(int, bassignblk, bstring* a, const void * s, int len);
#define bassignblk(a, s, len) BSTR_CALL(bassignblk, a, s, len)

/* Destroy function */
FORGE_API extern BSTR_DECLARE_FN(int, bdestroy , bstring* b);
#define bdestroy(b) BSTR_CALL(bdestroy, b)

/* Space allocation hinting functions */
FORGE_API extern BSTR_DECLARE_FN(int, balloc, bstring* s, int len);
#define balloc(s, len) BSTR_CALL(balloc, s, len)
FORGE_API extern BSTR_DECLARE_FN(int, ballocmin, bstring* b, int len);
#define ballocmin(b, len) BSTR_CALL(ballocmin, b, len)
/* The Forge additional allocation functions */
/* Makes string dynamic and allocates max(current capacity, len) bytes for buffer*/
FORGE_API extern BSTR_DECLARE_FN(int, bmakedynamic, bstring* b, int len);
#define bmakedynamic(b, len) BSTR_CALL(bmakedynamic, b, len)
/* Makes string dynamic and allocates minimal possible bytes to hold underlying string */
FORGE_API extern BSTR_DECLARE_FN(int, bmakedynamicmin, bstring* b, int len);
#define bmakedynamicmin(b, len) BSTR_CALL(bmakedynamicmin, b, len)


/* Substring extraction */
/* Use bassignmidstr instead
extern bstring bmidstr (const_bstring b, int left, int len);
*/

/* Various standard manipulations */
FORGE_API extern BSTR_DECLARE_FN(int, bconcat, bstring* b0, const bstring* b1);
#define bconcat(b0, b1) BSTR_CALL(bconcat, b0, b1)
FORGE_API extern BSTR_DECLARE_FN(int, bconchar, bstring* b0, char c);
#define bconchar(b0, c) BSTR_CALL(bconchar, b0, c)
FORGE_API extern BSTR_DECLARE_FN(int, bcatcstr, bstring* b, const char * s);
#define bcatcstr(b, s) BSTR_CALL(bcatcstr, b, s)
FORGE_API extern BSTR_DECLARE_FN(int, bcatblk, bstring* b, const void * s, int len);
#define bcatblk(b, s, len) BSTR_CALL(bcatblk, b, s, len)
FORGE_API extern BSTR_DECLARE_FN(int, binsert, bstring* s1, int pos, const bstring* s2, unsigned char fill);
#define binsert(s1, pos, s2, fill) BSTR_CALL(binsert, s1, pos, s2, fill)
FORGE_API extern BSTR_DECLARE_FN(int, binsertblk, bstring* s1, int pos, const void * s2, int len, unsigned char fill);
#define binsertblk(s1, pos, s2, len, fill) BSTR_CALL(binsertblk, s1, pos, s2, len, fill)
FORGE_API extern BSTR_DECLARE_FN(int, binsertch, bstring* s1, int pos, int len, unsigned char fill);
#define binsertch(s1, pos, len, fill) BSTR_CALL(binsertch, s1, pos, len, fill)
FORGE_API extern BSTR_DECLARE_FN(int, breplace, bstring* b1, int pos, int len, const bstring* b2, unsigned char fill);
#define breplace(b1, pos, len, b2, fill) BSTR_CALL(breplace, b1, pos, len, b2, fill)
FORGE_API extern BSTR_DECLARE_FN(int, bsetstr, bstring* b0, int pos, const bstring* b1, unsigned char fill);
#define bsetstr(b0, pos, b1, fill) BSTR_CALL(bsetstr, b0, pos, b1, fill) 
FORGE_API extern int bdelete(bstring* s1, int pos, int len);
FORGE_API extern BSTR_DECLARE_FN(int, btrunc, bstring* b, int n);
#define btrunc(b, n) BSTR_CALL(btrunc, b, n)

/* Scan/search functions */
FORGE_API extern int bstricmp (const bstring* b0, const bstring* b1);
FORGE_API extern int bstrnicmp (const bstring* b0, const bstring* b1, int n);
FORGE_API extern int bstrcmp (const bstring* b0, const bstring* b1);
FORGE_API extern int bstrncmp (const bstring* b0, const bstring* b1, int n);

FORGE_API extern int biseqcaseless (const bstring* b0, const bstring* b1);
FORGE_API extern int biseqcaselessblk (const bstring* b, const void * blk, int len);
FORGE_API extern int bisstemeqcaselessblk (const bstring* b0, const void * blk, int len);
FORGE_API extern int biseq (const bstring* b0, const bstring* b1);
FORGE_API extern int biseqblk (const bstring* b, const void * blk, int len);
FORGE_API extern int bisstemeqblk (const bstring* b0, const void * blk, int len);
FORGE_API extern int biseqcstr (const bstring* b, const char * s);
FORGE_API extern int biseqcstrcaseless (const bstring* b, const char * s);
FORGE_API extern int binstr (const bstring* s1, int pos, const bstring* s2);
FORGE_API extern int binstrr (const bstring* s1, int pos, const bstring* s2);
FORGE_API extern int binstrcaseless (const bstring* s1, int pos, const bstring* s2);
FORGE_API extern int binstrrcaseless (const bstring* s1, int pos, const bstring* s2);
FORGE_API extern int bstrchrp (const bstring* b, int c, int pos);
FORGE_API extern int bstrrchrp (const bstring* b, int c, int pos);
#define bstrchr(b,c) bstrchrp ((b), (c), 0)
#define bstrrchr(b,c) bstrrchrp ((b), (c), blength(b))
FORGE_API extern int binchr (const bstring* b0, int pos, const bstring* b1);
FORGE_API extern int binchrr (const bstring* b0, int pos, const bstring* b1);
FORGE_API extern int bninchr (const bstring* b0, int pos, const bstring* b1);
FORGE_API extern int bninchrr (const bstring* b0, int pos, const bstring* b1);
FORGE_API extern BSTR_DECLARE_FN(int, bfindreplace, bstring* b, const bstring* find, const bstring* repl, int pos);
#define bfindreplace(b, find, repl, pos) BSTR_CALL(bfindreplace, b, find, repl, pos)
FORGE_API extern BSTR_DECLARE_FN(int, bfindreplacecaseless, bstring* b, const bstring* find, const bstring* repl, int pos);
#define bfindreplacecaseless(b, find, repl, pos) BSTR_CALL(bfindreplacecaseless, b, find, repl, pos)

/* String split */
FORGE_API extern BSTR_DECLARE_FN(bstring*, bsplit, const bstring* str, unsigned char splitChar);
#define bsplit(str, splitChar) BSTR_CALL(bsplit, str, splitChar)
FORGE_API extern BSTR_DECLARE_FN(bstring*, bsplits, const bstring* str, const bstring* splitChars);
#define bsplits(str, splitChars) BSTR_CALL(bsplits, str, splitChars)
FORGE_API extern BSTR_DECLARE_FN(bstring*, bsplitstr, const bstring* str, const bstring* splitStr);
#define bsplitstr(str, splitStr) BSTR_CALL(bsplitstr, str, splitStr)

typedef int(*BSplitCallbackFn) (void * parm, int ofs, int len);

FORGE_API extern int bsplitcb(const bstring* str, unsigned char splitChar, int pos,
	BSplitCallbackFn cb, void * parm);
FORGE_API extern int bsplitscb(const bstring* str, const bstring* splitChars, int pos,
	BSplitCallbackFn cb, void * parm);
FORGE_API extern int bsplitstrcb(const bstring* str, const bstring* splitStr, int pos,
	BSplitCallbackFn cb, void * parm);

/* String join */
FORGE_API extern BSTR_DECLARE_FN(int, bjoin, bstring* out, const bstring inputs[], int count, const bstring* sep);
#define bjoin(out, inputs, count, sep) BSTR_CALL(bjoin, out, inputs, count, sep)
FORGE_API extern BSTR_DECLARE_FN(int, bjoinblk, bstring* out, const bstring inputs[], int count, const void * sep, int sepLen);
#define bjoinblk(out, inputs, count, sep, sepLen) BSTR_CALL(bjoinblk, out, inputs, count, sep, sepLen)


/* Miscellaneous functions */
FORGE_API extern BSTR_DECLARE_FN(int, bpattern, bstring* b, int len);
#define bpattern(b, len) BSTR_CALL(bpattern, b, len)
FORGE_API extern int btoupper (bstring* b);
FORGE_API extern int btolower (bstring* b);
FORGE_API extern int bltrimws (bstring* b);
FORGE_API extern int brtrimws (bstring* b);
FORGE_API extern int btrimws (bstring* b);

#if !defined (BSTRLIB_NOVSNP)
FORGE_API extern BSTR_DECLARE_FN(int, bformat, bstring* b, const char * fmt, ...);
#define bformat(b, fmt, ...) BSTR_CALL(bformat, b, fmt OPT_COMMA_VA_ARGS(__VA_ARGS__))
FORGE_API extern BSTR_DECLARE_FN(int, bformata , bstring* b, const char * fmt, ...);
#define bformata(b, fmt, ...) BSTR_CALL(bformata, b, fmt OPT_COMMA_VA_ARGS(__VA_ARGS__))
FORGE_API extern BSTR_DECLARE_FN(int, bvformat, bstring* b, const char* fmt, va_list args);
#define bvformat(b, fmt, args) BSTR_CALL(bvformat, b, fmt, args)
FORGE_API extern BSTR_DECLARE_FN(int, bvformata, bstring* b, const char* fmt, va_list args);
#define bvformata(b, fmt, args) BSTR_CALL(bvformata, b, fmt, args)
#endif

/* Macros for string literals */
/* Creates heap allocated string from string literal with minimal capacity of m */
#define bdynallocfromliteral(str, m) bdynfromblk( ("" str ""), sizeof(str) - 1, m)
/* Creates heap allocated string from string literal */
#define bdynfromliteral(str)  bdynallocfromliteral(str, 1)



#define bassignliteral(b, str)              (bassignblk((b), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define bcatliteral(b, str)                 (bcatblk((b), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define binsertliteral(b, pos, str, fill)   (binsertblk((b), (pos), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define bjoinliteral(b, inputs, count, str) (bjoinblk((b), (inputs), (count), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define biseqliteral(b, str)                (biseqblk((b), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define bisstemeqliteral(b, str)            (bisstemeqblk((b), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define biseqcaselessliteral(b, str)        (biseqcaselessblk((b), ((void *)("" str "")), ((int) sizeof(str)-1)))
#define bisstemeqcaselessliteral(b,str)     (bisstemeqcaselessblk((b), ((void *)("" str "")), ((int) sizeof(str)-1)))


/* Reference building functions and macros */
/* Initializes string that references raw block of data */
#ifdef __cplusplus
#define bconstfromblk(blk, size) { 0, size, (unsigned char*)(blk) }
#else
#define bconstfromblk(blk, size) (bstring){ 0, size, (unsigned char*)(blk) }
#endif
/* Initializes string that references string literal */
#define bconstfromliteral(str) bconstfromblk( ("" str ""), sizeof(str) - 1)
/* Initializes string that references a C string */
#define bconstfromcstr(str) bconstfromblk( (str) , strlen(str) )
/* Initializes string that references the same buffer as input string */
#define bconstfromstr(str) bconstfromblk(str->data, str->slen)


#ifdef __cplusplus
}
#endif

#endif
