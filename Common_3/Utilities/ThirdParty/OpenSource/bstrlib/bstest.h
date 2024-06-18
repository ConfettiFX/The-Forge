/*
 * This source file is part of the bstring* string library.  This code was
 * written by Paul Hsieh in 2002-2015, and is covered by the BSD open source
 * license. Refer to the accompanying documentation for details on usage and
 * license.
 */

/*
 * bstest.c
 *
 * This file is the C unit test for Bstrlib.
 */

// This warning means that the `cond` in `if (cond) { ... }` is known
// at compile time. Since this file is for testing bstrlib, there are
// many test asserts that are known at compile time that trigger this warning,
// so we disable it for this file only.
#ifdef _MSC_VER
#pragma warning(disable: 4127) // conditional expression is constant
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include "bstrlib.h"
//#include "bstraux.h"

#include "../Nothings/stb_ds.h"

#include "../../../../Utilities/Threading/Atomics.h"
#include "../../../../Utilities/Interfaces/ILog.h"
#include "../../../../Utilities/Interfaces/IThread.h"

#define DUMP_BSTRING_COUNT 32
static bstring dumpOut[DUMP_BSTRING_COUNT];
static int rot = 0;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstring-plus-int"
#elif defined(_MSC_VER)
#pragma warning(disable: 4130)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * All returns from bstring functions have additional constraints
 */
static int correctBstring (const bstring * b) {

	return bisvalid(b) && bmlen(b) > b->slen && b->data[b->slen] == '\0';
}

#define BSTR_LOG_EXPECT_ERR(function, input, expected, output) { \
	LOGF(eERROR, "  Expected: %s(%s) = %s", function, input, expected); \
    LOGF(eERROR, "       Got: %s(%s) = %s", function, input, output); }

#define printablecstr(str) ((str) ? ((str)[0] ? (str) : "<EMPTY>") : "<NULL>")

#define ARR_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

static const char * dumpBstringImpl(const bstring * b, bstring* out)
{
	int rv;
	char tmp[256];

	ASSERT(bisvalid(out));

	if (b == NULL)
		bcatliteral(out, "NULL");
	else
	{
		rv = snprintf(tmp, sizeof(tmp), "%p", b);
		ASSERT(rv >= 0 && rv < (int)sizeof(tmp));
		bcatcstr(out, tmp);
		bconchar(out, ':');

		if (bconstisvalid(b))
		{
			balloc(out, out->slen + b->slen + 10);

			bconchar(out, '"');
			for (int i = 0; i < b->slen; ++i)
			{
				char ch = (char)b->data[i];
				if (ch != '\0')
					bconchar(out, ch);
				else
					bcatliteral(out, "\\0");
			}

			if (b->data[b->slen] == '\0')
				bcatliteral(out, "\\0");

			bconchar(out, '"');
			
			if (!bisvalid(b))
			{
				rv = snprintf(tmp, sizeof(tmp), "[mlen:%d bmlen:%d slen:%d]", b->mlen, bmlen(b), b->slen);
				ASSERT(rv >= 0 && rv < (int)sizeof(tmp));
				bcatcstr(out, tmp);
			}
			else 
			{
				if (bownsdata(b))
					bcatliteral(out, "[dynamic]");
				else
					bcatliteral(out, "[static]");
			}
		}
		else
		{
			rv = snprintf(tmp, sizeof(tmp), "[data:%p mlen:%d bmlen:%d slen:%d]", b->data, b->mlen, bmlen(b), b->slen);
			ASSERT(rv >= 0 && rv < (int)sizeof(tmp));
			bcatcstr(out, tmp);
		}
	}
	ASSERT(biscstr(out));

	return (char*)out->data;
}

static const char* dumpBstring(const bstring* b)
{
	rot = (rot + 1) % (unsigned)DUMP_BSTRING_COUNT;

	bstring* out = &dumpOut[rot];
	if (out->data == NULL) {
		*out = bempty();
	}
	out->slen = 0;
	out->data[out->slen] = '\0';
	return dumpBstringImpl(b, out);
}

static const char * dumpBstringArr(const bstring* arr, int len)
{
	if (arr == NULL)
		return "NULL";
	if (len == 0)
		return "{}";

	rot = (rot + 1) % (unsigned)DUMP_BSTRING_COUNT;

	bstring* out = &dumpOut[rot];
	if (out->data == NULL) {
		*out = bempty();
	}
	out->slen = 0;
	out->data[out->slen] = '\0';

	bconchar(out, '{');
	for (int i = 0; i < len; ++i)
	{
		dumpBstringImpl(&arr[i], out);
		bcatliteral(out, ", ");
	}
	bconchar(out, '}');
	return (char*)out->data;
}

static const char* dumpCstring (const char* s) 
{
	rot = (rot + 1) % (unsigned)DUMP_BSTRING_COUNT;
	if (dumpOut[rot].data == NULL) {
		dumpOut[rot] = bempty();
	}
	dumpOut[rot].slen = 0;
	if (s == NULL) {
		bcatcstr (&dumpOut[rot], "NULL");
	} else {
		static char msg[64];
		int i;

		snprintf (msg, 64, "cstr[%p] -> ", (void *)s);
		bcatcstr (&dumpOut[rot], msg);

		bcatliteral(&dumpOut[rot], "\"");
		for (i = 0; s[i]; i++) {
			if (i > 1024) {
				bcatliteral(&dumpOut[rot], " ...");
				break;
			}
			bconchar (&dumpOut[rot], s[i]);
		}
		bcatliteral(&dumpOut[rot], "\"");
	}

	return (char *) dumpOut[rot].data;
}


/*
 * GLOBALS
 */

#define EMPTY_STRING ""
#define SHORT_STRING "bogus"
#define EIGHT_CHAR_STRING "Waterloo"
#define LONG_STRING  "This is a bogus but reasonably long string.  Just long enough to cause some mallocing."
#define LONG_STRING_2 "This is a bogus but reasonably long string.\0  Just long enough to cause some mallocing."

static const char* cStrings[] =
{
	EMPTY_STRING,
	SHORT_STRING,
	EIGHT_CHAR_STRING,
	LONG_STRING,
	LONG_STRING_2
};

static const int cStringSizes[] =
{
	(int)sizeof(EMPTY_STRING),
	(int)sizeof(SHORT_STRING),
	(int)sizeof(EIGHT_CHAR_STRING),
	(int)sizeof(LONG_STRING),
	(int)sizeof(LONG_STRING_2),
};

COMPILE_ASSERT(ARR_SIZE(cStrings) == ARR_SIZE(cStringSizes));

static const bstring goodConstBstrings[] =
{
	bconstfromliteral(EMPTY_STRING),
	bconstfromliteral(SHORT_STRING),
	bconstfromliteral(EIGHT_CHAR_STRING),
	bconstfromliteral(LONG_STRING),
	bconstfromliteral(LONG_STRING_2),
	bconstfromliteral(LONG_STRING "\0\0\0"),
	bempty(),
	{ 0, 5,  (unsigned char *)SHORT_STRING},
	{ 5, 5,  (unsigned char *)SHORT_STRING},
	{-5, 5, (unsigned char *)SHORT_STRING},
};

static const bstring emptyBstring = bconstfromliteral(EMPTY_STRING);
static const bstring shortBstring = bconstfromliteral(SHORT_STRING);
static const bstring eightCharBstring = bconstfromliteral(EIGHT_CHAR_STRING);
static const bstring longBstring = bconstfromliteral(LONG_STRING);
static const bstring longBstring2 = bconstfromliteral(LONG_STRING_2);

static const bstring goodConstBstringsEq[] =
{
	bconstfromliteral(EMPTY_STRING),
	bconstfromliteral(SHORT_STRING),
	bconstfromliteral(EIGHT_CHAR_STRING),
	bconstfromliteral(LONG_STRING),
	bconstfromliteral(LONG_STRING_2),
	bempty(),
	{ 0, 5,  (unsigned char *)SHORT_STRING},
	{ 5, 5,  (unsigned char *)SHORT_STRING},
	{-5, 5, (unsigned char *)SHORT_STRING},

};


static const bstring badConstBstrings[] =
{
	{10,  10, NULL},
	{0,  0, NULL },
	{0,  -1, NULL },
	{2, -5, (unsigned char *)SHORT_STRING},
	{0, -5, (unsigned char *)SHORT_STRING },
	{-5, -5, (unsigned char *)SHORT_STRING },
};

/* 
 * Bad bstrings should never be modified
 * so all buffers point to const memory or NULL
 * in order to cause seg fault if any gets modified
 */
static const bstring badBstrings[] =
{
	badConstBstrings[0],
	badConstBstrings[1],
	badConstBstrings[2],
	badConstBstrings[3],
	badConstBstrings[4],
	badConstBstrings[5],
	{0, 1, NULL},
	{0, 0, NULL},
	{0, 1, (unsigned char *)SHORT_STRING},
	{0, 0, (unsigned char *)SHORT_STRING}
};

#define bstringFromLiteral(x) { sizeof(x), sizeof(x) - 1, (unsigned char*)("" x "") }
#define bstringFromLiteral2(x) { sizeof(x) - 1, sizeof(x) - 1, (unsigned char*)("" x "") }

/* These bstrings should be used in fail scenarios, to ensure that they are never modified */
static const bstring pseudoGoodBstrings[] =
{
	bstringFromLiteral(EMPTY_STRING),
	bstringFromLiteral(SHORT_STRING),
	bstringFromLiteral(EIGHT_CHAR_STRING),
	bstringFromLiteral(LONG_STRING),
	bstringFromLiteral(LONG_STRING_2),
	bstringFromLiteral2(SHORT_STRING),
	bstringFromLiteral2(EIGHT_CHAR_STRING),
	bstringFromLiteral2(LONG_STRING),
	bstringFromLiteral2(LONG_STRING_2),
};


#define MEM_PATTERN 0xfa
#define MEM_OFS 8
#define STATIC_BUF_SIZE 256

#define STATIC_BUF_COUNT (ARR_SIZE(cStrings) * 4)
#define TOTAL_GOOD_STRING_COUNT (STATIC_BUF_COUNT*2 + 1)



/*
 * Initializes different all of the following combinations of writable strings:
 * + statically/dynamically allocated
 * + null terminated or not
 * + with spare capacity and not
 * + strings with null terminator in the middle
*/
static void initGoodBstrings(unsigned char (*staticBufs)[STATIC_BUF_COUNT][STATIC_BUF_SIZE], 
	                         bstring(*out)[TOTAL_GOOD_STRING_COUNT])
{
	unsigned char* pBuf;
	int bufi = 0;
	for (int i = 0; i < (int)STATIC_BUF_COUNT; ++i)
		memset(&(*staticBufs)[i][0], MEM_PATTERN, STATIC_BUF_SIZE);

	/* static strings */

	/* Null terminated static strings with slen + 1 capacity */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i];
		ASSERT(len + MEM_OFS < STATIC_BUF_SIZE);
		pBuf = &(*staticBufs)[bufi][0] + MEM_OFS;
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len - 1;
		(*out)[bufi].mlen = len;
	}

	/* NOT null terminated static strings with slen capacity */
	/* Note: empty string mlen should be at least 1*/
	pBuf = &(*staticBufs)[bufi][0] + MEM_OFS;
	(*out)[bufi].data = pBuf;
	(*out)[bufi].slen = 0;
	(*out)[bufi].mlen = 1;
	++bufi;

	for (int i = 1; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i] - 1;
		ASSERT(len >= 1);
		ASSERT(len + MEM_OFS < STATIC_BUF_SIZE);
		pBuf = &(*staticBufs)[bufi][0] + MEM_OFS;
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len;
		(*out)[bufi].mlen = len;
	}


	/* Null terminated static strings with slen + 100 capacity */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i];
		ASSERT(len + 100 + MEM_OFS < STATIC_BUF_SIZE);
		pBuf = &(*staticBufs)[bufi][0] + MEM_OFS;
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len - 1;  /* exclude null terminator */
		(*out)[bufi].mlen = len + 100;
	}

	/* NOT null terminated static strings with slen + 100 capacity */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i] - 1;
		pBuf = &(*staticBufs)[bufi][0] + MEM_OFS;
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len;
		(*out)[bufi].mlen = len + 100;
	}


	/* Dynamic strings */

	/* Null terminated dynamic strings with slen + 1 capacity */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i];
		pBuf = (unsigned char*)tf_malloc(len);
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len - 1;
		(*out)[bufi].mlen = len | BSTR_DYNAMIC_BIT;
	}

	/* NOT null terminated dynamic strings with slen capacity */
	/* Note: empty string mlen should be at least 1*/
	pBuf = (unsigned char*)tf_malloc(1);
	(*out)[bufi].data = pBuf;
	(*out)[bufi].slen = 0;
	(*out)[bufi].mlen = 1 | BSTR_DYNAMIC_BIT;
	++bufi;

	for (int i = 1; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i] - 1;
		ASSERT(len >= 1);
		pBuf = (unsigned char*)tf_malloc(len);
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len;
		(*out)[bufi].mlen = len | BSTR_DYNAMIC_BIT;
	}


	/* Null terminated dynamic strings with slen + 100 capacity */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i];
		pBuf = (unsigned char*)tf_malloc(len + 100);
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len - 1;  /* exclude null terminator */
		(*out)[bufi].mlen = (len + 100) | BSTR_DYNAMIC_BIT;
	}

	/* NOT null terminated dynamic strings with slen + 100 capacity */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i, ++bufi)
	{
		int len = cStringSizes[i] - 1;
		pBuf = (unsigned char*)tf_malloc(len + 100);
		memcpy(pBuf, cStrings[i], len);
		(*out)[bufi].data = pBuf;
		(*out)[bufi].slen = len;
		(*out)[bufi].mlen = (len + 100) | BSTR_DYNAMIC_BIT;
	}

	/* global empty string */
	(*out)[TOTAL_GOOD_STRING_COUNT - 1] = bempty();

	for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
	{
		bstring* str = &(*out)[i];
		ASSERT(bisvalid(str));
		ASSERT((i < (int)TOTAL_GOOD_STRING_COUNT / 2 && !bownsdata(str) ) || bownsdata(str) || i == ((int)TOTAL_GOOD_STRING_COUNT - 1));
	}
}

static void deinitGoodBstrings(unsigned char(*staticBufs)[STATIC_BUF_COUNT][STATIC_BUF_SIZE],
                               bstring(*strings)[TOTAL_GOOD_STRING_COUNT])
{
	unsigned char pattern[STATIC_BUF_SIZE];
	memset(pattern, MEM_PATTERN, STATIC_BUF_SIZE);
	/* Check that prefix wasn't modified */
	for (int i = 0; i < (int)STATIC_BUF_COUNT; ++i)
	{
		unsigned char* buf = &(*staticBufs)[i][0];
		ASSERT(memcmp(buf, pattern, MEM_OFS) == 0);
	}

	/* loop through tight buffers, but skip empty str */
	for (int i = 1; i < (int)ARR_SIZE(cStrings); ++i)
	{
		int index = i + ARR_SIZE(cStrings);
		unsigned char* buf = &(*staticBufs)[index][0];
		size_t ofs = MEM_OFS + cStringSizes[i] - 1;
		ASSERT(memcmp(buf + ofs, pattern, STATIC_BUF_SIZE - ofs) == 0);
	}


	for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
	{
		bstring* str = &(*strings)[i];
		bdestroy(str);
	}
}


/*
 *  BSTRING CREATION FUNCTIONS
 */

static int test0_0_bdynfromcstr (const char * s, const char * res) 
{
	bstring b0 = bdynfromcstr(s);
	int ret = 0;

	if (correctBstring(&b0))
	{
		ret += (res == NULL) || ((int)strlen(res) != b0.slen)
			|| (0 != memcmp(res, b0.data, b0.slen));
		/* Empty and invalid string should not be allocated on the heap */
		if (res == NULL || *res == '\0')
			ret += bownsdata(&b0);
		else
			ret += !bownsdata(&b0);
	}
	else if (res)
		++ret;
	
	s = printablecstr(s);
	res = printablecstr(res);

	if (ret)
		BSTR_LOG_EXPECT_ERR("bdynfromcstr", s, res, dumpBstring(&b0));

	bdestroy (&b0);
	return ret;
}

static int test0_1_bdynallocfromcstr(const char * s, int minCap, const char * res) 
{
	bstring b0 = bdynallocfromcstr(s, minCap);
	int ret = 0;


	if (correctBstring(&b0))
	{
		ret += (res == NULL) || ((int)strlen(res) != b0.slen)
			|| (0 != memcmp(res, b0.data, b0.slen));
		ret += minCap > bmlen(&b0);
		/* Empty and invalid string should not be allocated on the heap */
		if (minCap <= 1 && ( res == NULL || *res == '\0'))
			ret += bownsdata(&b0);
		else
			ret += !bownsdata(&b0);
	} 
	else if (res)
		++ret;

	s = printablecstr(s);
	res = printablecstr(res);

	if (ret)
		BSTR_LOG_EXPECT_ERR("bdynallocfromcstr", s, res, dumpBstring(&b0));

	bdestroy (&b0);
	return ret;
}


static int test0 (void) {
	int ret = 0;

	LOGF(eINFO, "TEST 0: bstring bdynfromcstr(const char* str);");

	/* tests with NULL */
	ret += test0_0_bdynfromcstr(NULL, NULL);

	/* normal operation tests */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i)
	{
		const char* cstr = cStrings[i];
		ret += test0_0_bdynfromcstr(cstr, cstr);
	}
	LOGF(eINFO, "\t# failures: %d", ret);

	LOGF(eINFO, "TEST 0: bstring bdynallocfromcstr(const char* str, int minCapacity);");

	/* tests with NULL */
	ret += test0_1_bdynallocfromcstr (NULL,  0, NULL);
	ret += test0_1_bdynallocfromcstr(NULL, 1, NULL);
	ret += test0_1_bdynallocfromcstr(NULL, -1, NULL);
	ret += test0_1_bdynallocfromcstr(NULL, 100, NULL);

	/* normal operation tests */
	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i)
	{
		const char* cstr = cStrings[i];
		ret += test0_1_bdynallocfromcstr(cstr, 0, cstr);
		ret += test0_1_bdynallocfromcstr(cstr, 1, cstr);
		ret += test0_1_bdynallocfromcstr(cstr, -1, NULL);
		ret += test0_1_bdynallocfromcstr(cstr, 100, cstr);
	}

	LOGF(eINFO, "\t# failures: %d", ret);

	return ret;
}

static int test1_0_bdynfromblk (const void * blk, int len, int minCap, const char * res) {
	bstring b0 = bdynfromblk(blk, len, minCap);
	int ret = 0;

	if (correctBstring(&b0))
	{

		ret += (res == NULL) || (len != b0.slen)
			|| (0 != memcmp(res, b0.data, len));
		ret += minCap > bmlen(&b0);

		/* Empty and invalid string should not be allocated on the heap */
		if (minCap <= 1 && (res == NULL || *res == '\0'))
			ret += bownsdata(&b0);
		else
			ret += !bownsdata(&b0);

	}
	else if (res)
		++ret;

	if (ret)
		BSTR_LOG_EXPECT_ERR("bdynfromblk", "<void*>", res, dumpBstring(&b0));

	
	bdestroy(&b0);
	return ret;
}

static int test1 (void) {
int ret = 0;

	LOGF(eINFO, "TEST 1: bstring bdynfromblk (const void * blk, int len, int minCapacity);");

	/* tests with NULL */
	ret += test1_0_bdynfromblk (NULL, 10, 0, NULL);
	ret += test1_0_bdynfromblk(NULL, 10, 10, NULL);
	ret += test1_0_bdynfromblk(NULL, 10, -1, NULL);

	ret += test1_0_bdynfromblk(NULL, 0, 0,  NULL);
	ret += test1_0_bdynfromblk(NULL, 0, 10, NULL);
	ret += test1_0_bdynfromblk(NULL, 0, -1, NULL);

	ret += test1_0_bdynfromblk (NULL, -1, 0, NULL);
	ret += test1_0_bdynfromblk(NULL, -1, 10, NULL);
	ret += test1_0_bdynfromblk(NULL, -1, -1, NULL);

	/* Invalid lengths */
	ret += test1_0_bdynfromblk(EMPTY_STRING, -1, 0, NULL);
	ret += test1_0_bdynfromblk(EMPTY_STRING, -1, 1, NULL);
	ret += test1_0_bdynfromblk(EMPTY_STRING, -1, -1, NULL);
	ret += test1_0_bdynfromblk(SHORT_STRING, -1, 0, NULL);
	ret += test1_0_bdynfromblk(SHORT_STRING, -1, 1, NULL);
	ret += test1_0_bdynfromblk(SHORT_STRING, -1, -1, NULL);


	/* normal operation tests */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const char* blk = (const char*)goodConstBstrings[i].data;
		int len = goodConstBstrings[i].slen;

		ret += test1_0_bdynfromblk(blk, len, 0, blk);
		ret += test1_0_bdynfromblk(blk, len, 1, blk);
		ret += test1_0_bdynfromblk(blk, len, 10, blk);
		ret += test1_0_bdynfromblk(blk, len, 100, blk);

		ret += test1_0_bdynfromblk(blk, len, -1, NULL);
	}

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}


static int test2_0_bdynfromstr(const bstring* from, int minCap, const bstring* res)
{
	int ret = 0;
	bstring b0 = bdynfromstr(from, minCap);
	if (correctBstring(&b0))
	{
		ret += !bconstisvalid(res) || b0.slen != res->slen || (0 != memcmp(res->data, b0.data, res->slen));
		ret += minCap > bmlen(&b0);

		/* Empty and invalid string should not be allocated on the heap */
		if (minCap <= 1 && (!bconstisvalid(res) || res->data[0] == '\0'))
			ret += bownsdata(&b0);
		else
			ret += !bownsdata(&b0);

	}
	else if (res)
		++ret;


	if (ret)
		BSTR_LOG_EXPECT_ERR("bdynfromstr", dumpBstring(from), dumpBstring(res), dumpBstring(&b0));


	bdestroy(&b0);
	return ret;
	
}

static int test2(void)
{
	int ret = 0;

	LOGF(eINFO, "TEST 2: bstring bdynfromstr(const bstring* b1, int minCapacity);");


	/* Null pointer testing */
	ret += test2_0_bdynfromstr(NULL, 0, NULL);
	ret += test2_0_bdynfromstr(NULL, -1, NULL);
	ret += test2_0_bdynfromstr(NULL, 10, NULL);

	/* Invalid string testing */
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		ret += test2_0_bdynfromstr(&badConstBstrings[i], 0, NULL);
		ret += test2_0_bdynfromstr(&badConstBstrings[i], -1, NULL);
		ret += test2_0_bdynfromstr(&badConstBstrings[i], 10, NULL);
	}

	/* Valid string testing */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		ret += test2_0_bdynfromstr(&goodConstBstrings[i], -1, NULL);
		ret += test2_0_bdynfromstr(&goodConstBstrings[i], 0,  &goodConstBstrings[i]);
		ret += test2_0_bdynfromstr(&goodConstBstrings[i], 10, &goodConstBstrings[i]);
		ret += test2_0_bdynfromstr(&goodConstBstrings[i], 100, &goodConstBstrings[i]);
	}

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}




static int test3_0_bassign(bstring* b0, const bstring* b1, const void* res, int minMlen, int maxMlen, int resSlen, int expectRV)
{
	int ret = 0;

	bstring b0copy = { 0,0,NULL };

	if (expectRV != BSTR_OK)
		b0copy = *b0;
	else
		b0copy = bdynfromstr(b0, 0);

	int rv = bassign(b0, b1);

	ret += rv != expectRV;
	if (res)
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b0);
		
		ret += !bisvalid(b0) || b0->slen != resSlen || bmlen(b0) < minMlen ||
			bmlen(b0) > maxMlen || memcmp(b0->data, res, resSlen) != 0;
	} 
	else
	{
		ret += bisvalid(b0);
		ret += b0->slen != b0copy.slen || b0->mlen != b0copy.mlen || b0->data != b0copy.data;
	}
		

	if (ret)
	{
		bstring resStr = { minMlen, resSlen, (unsigned char*)res };
		LOGF(eERROR, "  Expected: bassign(%s,%s) = %s", dumpBstring(&b0copy), dumpBstring(b1), dumpBstring(&resStr) );
		LOGF(eERROR, "       Got: bassign(%s,%s) = %s", dumpBstring(&b0copy), dumpBstring(b1), dumpBstring(b0));
	}

	if (bisvalid(&b0copy) && b0copy.data != b0->data)
		bdestroy(&b0copy);

	return ret;
}



static int test3()
{
	int ret = 0;

	LOGF(eINFO, "TEST 3: int bassign (bstring* a, const bstring* b);");

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			bstring* out = (bstring*)&badBstrings[i];
			const bstring* in = (bstring*)&badConstBstrings[j];
			ret += test3_0_bassign(out, in, NULL, INT_MIN, INT_MAX, 0, BSTR_ASSERT_ERR);
		}
		for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
		{
			bstring* out = (bstring*)&badBstrings[i];
			const bstring* in = (bstring*)&goodConstBstrings[j];
			ret += test3_0_bassign(out, in, NULL, INT_MIN, INT_MAX, 0, BSTR_ASSERT_ERR);
		}
	}

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];
	initGoodBstrings(&staticBufs, &strings);

	for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
	{
		for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
		{
			bstring * out = (bstring*)&strings[i];
			const bstring* in = (bstring*)&badConstBstrings[j];
			bstring copy = bdynfromstr(out, 0);
			ASSERT(correctBstring(&copy));
			ret += test3_0_bassign(out, in, copy.data, bmlen(out), bmlen(out), copy.slen, BSTR_ASSERT_ERR);
			bdestroy(&copy);
		}
	}
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
	{
		for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
		{
			bstring* out = (bstring*)&strings[i];
			const bstring* in = (bstring*)&goodConstBstrings[j];
			bstring copy = bdynfromstr(out, 0);
			ret += test3_0_bassign(out, in, in->data, in->slen + 1, INT_MAX, in->slen, BSTR_OK);
			bdestroy(&copy);
		}
	}
	deinitGoodBstrings(&staticBufs, &strings);
	initGoodBstrings(&staticBufs, &strings);

	for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
	{
		bstring* out = (bstring*)&strings[i];
		bstring copy = bdynfromstr(out, 0);
		if (bmlen(out) == out->slen)
			ret += test3_0_bassign(out, out, copy.data, out->slen + 1, INT_MAX, copy.slen, BSTR_OK);
		else
			ret += test3_0_bassign(out, out, copy.data, bmlen(out), bmlen(out), copy.slen, BSTR_OK);
		bdestroy(&copy);
	}

	deinitGoodBstrings(&staticBufs, &strings);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test4_0(bstring* b0, const bstring* res, int expectRV)
{
	int ret = 0;


	bstring copy = *b0;
	if (bisvalid(b0))
	{
		copy = bdynfromstr(b0, 0);
	}

	int rv = bmakecstr(b0);

	ret += rv != expectRV;

	if (bconstisvalid(res))
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b0);
		ret += b0->slen != res->slen ||
			(0 != memcmp(res->data, b0->data, res->slen));
	}
	else
	{
		ret += bisvalid(b0);
		ret += b0->slen != copy.slen || b0->mlen != copy.mlen || b0->data != copy.data;
	}

	if (ret)
		BSTR_LOG_EXPECT_ERR("bmakecstr", dumpBstring(&copy), dumpBstring(res), dumpBstring(b0));

	if (bisvalid(&copy) && copy.data != b0->data)
		bdestroy(&copy);

	return ret;
}

static int test4()
{
	int ret = 0;
	LOGF(eINFO, "TEST 4: int bmakecstr(bstring* b);");

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test4_0(str, NULL, BSTR_ASSERT_ERR);
	}

	unsigned char staticBuffers[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];
	initGoodBstrings(&staticBuffers, &strings);

	unsigned char copyBuf[256];
	bstring copy = bemptyfromarr(copyBuf);

	for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
	{
		bstring* str = &strings[i];
		int res = bassign(&copy, str);
		ASSERT(res == BSTR_OK && copy.data[copy.slen] == '\0');
		ret += test4_0(str, &copy, BSTR_OK);
	}

	deinitGoodBstrings(&staticBuffers, &strings);
	bdestroy(&copy);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;

}


static int test5_0 (bstring* b0, const bstring* b1, int expect) 
{

	unsigned char buf0[256];
	bstring copy0 = { 0,0,NULL };

	unsigned char buf1[256];
	bstring copy1 = { 0,0,NULL };

	int rv, ret = 0;

	if (bisvalid(b0))
	{
		copy0 = bemptyfromarr(buf0);
		bassign(&copy0, b0);
	}
	else
		copy0 = b0 ? *b0 : copy0;

	if (bisvalid(b1))
	{
		copy1 = bemptyfromarr(buf1);
		bassign(&copy1, b1);
	}
	else
		copy1 = b1 ? *b1 : copy1;

	rv = bconcat(b0, b1);

	if (rv == BSTR_OK && rv == expect)
	{
		ret += !correctBstring(b0) || b0->slen != copy0.slen + copy1.slen ||
			   memcmp(b0->data, copy0.data, copy0.slen) != 0 ||
			   memcmp(b0->data + copy0.slen, copy1.data, copy1.slen) != 0;
	}
	else if (rv == BSTR_ASSERT_ERR && rv == expect)
	{
		if (bisvalid(&copy0))
		{
			ret += !bisvalid(b0) || b0->slen != copy0.slen ||
				memcmp(b0->data, copy0.data, copy0.slen) != 0;
		}
		else
		{
			ret += bisvalid(b0) && memcmp(b0, &copy0, sizeof(*b0)) != 0;
		}
			
			       
	}
	else
		++ret;

	if (ret) {
		LOGF(eERROR, "  Error: bconcat(%s, %s) = %s", dumpBstring(&copy0), dumpBstring(&copy1), dumpBstring(b0));
	}

	bdestroy(&copy0);
	bdestroy(&copy1);

	return ret;
}

static int test5 (void) {
	int ret = 0;

	LOGF(eINFO, "TEST 5: int bconcat (bstring* b0, const bstring* b1);");


	/* tests with NULL */
	ret += test5_0 (NULL, NULL, BSTR_ASSERT_ERR);
	ret += test5_0 (NULL, &goodConstBstrings[0], BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b0 = (bstring*)&pseudoGoodBstrings[i];
		ret += test5_0(b0, NULL, BSTR_ASSERT_ERR);
	}

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* b0 = (bstring*)&badBstrings[i];

		ret += test5_0(b0, NULL, BSTR_ASSERT_ERR);

		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = (bstring*)&badConstBstrings[j];
			ret += test5_0(b0, b1, BSTR_ASSERT_ERR);
		}

		for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
		{
			const bstring* b1 = (bstring*)&goodConstBstrings[j];
			ret += test5_0(b0, b1, BSTR_ASSERT_ERR);
		}
	}

	unsigned char staticBuffers[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	unsigned char copyBuf[STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];
	initGoodBstrings(&staticBuffers, &strings);

	for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
	{
		const bstring* b1 = (bstring*)&badConstBstrings[j];
		for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
		{
			bstring* b0 = &strings[i];
			bstring copy = bemptyfromarr(copyBuf);
			bassign(&copy, b0);
			ret += test5_0(b0, b1, BSTR_ASSERT_ERR);
		}
	}

	deinitGoodBstrings(&staticBuffers, &strings);
	initGoodBstrings(&staticBuffers, &strings);


	for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
	{
		const bstring* b1 = (bstring*)&goodConstBstrings[j];
		for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
		{
			bstring* b0 = &strings[i];
			bstring copy = bemptyfromarr(copyBuf);
			bassign(&copy, b0);

			ret += test5_0(b0, b1, BSTR_OK);
			bassign(b0, &copy);
		}
	}

	deinitGoodBstrings(&staticBuffers, &strings);
	initGoodBstrings(&staticBuffers, &strings);

	/* Aliasing */
	for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
	{
		bstring* b0 = &strings[i];
		bstring copy = bemptyfromarr(copyBuf);
		bassign(&copy, b0);

		bstring alias = *b0;
		ret += test5_0(b0, &alias, BSTR_OK);
	}


	deinitGoodBstrings(&staticBuffers, &strings);
	initGoodBstrings(&staticBuffers, &strings);

	for (int i = 0; i < (int)TOTAL_GOOD_STRING_COUNT; ++i)
	{
		bstring* b0 = &strings[i];

		bstring alias = *b0;

		if (b0->slen > 1)
		{
			bstring copy = bemptyfromarr(copyBuf);
			bassign(&copy, b0);

			alias = *b0;
			alias.data += 2;
			alias.slen -= 4;
			ASSERT(alias.slen > 0);
			ret += test5_0(b0, &alias, BSTR_OK);
		}
	}

	deinitGoodBstrings(&staticBuffers, &strings);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test6_0 (bstring* b, char c, const void * res, int resLen, int expectRV) {
	int rv, ret = 0;

	bstring copy = bdynfromstr(b, 256);

	rv = bconchar(b, c);
	ret += rv != expectRV;

	if (res)
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b);
		
		ret += !bisvalid(b) || b->slen != resLen || memcmp(b->data, res, resLen) != 0;
	}
	else
		ret += bisvalid(b);
	
	if (ret)
	{
		char charBuf[3] = { c,0,0 };
		if (c == '\0')
		{
			charBuf[0] = '\\';
			charBuf[1] = '0';
		}

		bassigncstr(&copy, dumpBstring(&copy));
		bcatliteral(&copy, ", ");
		bcatcstr(&copy, charBuf);

		bstring expect = { 0, resLen, (unsigned char*)res };

		BSTR_LOG_EXPECT_ERR("bconchar", dumpBstring(&copy), dumpBstring(&expect), dumpBstring(b));
	}

	bdestroy(&copy);
	
	return ret;
}

static int test6 (void) {
	int ret = 0;

	LOGF(eINFO, "TEST 6: int bconchar (bstring* b, char c);");

	/* test with NULL */
	ret += test6_0 (NULL, (char) 'x', NULL, 0, BSTR_ASSERT_ERR);
	ret += test6_0(NULL, (char) '\0', NULL, 0, BSTR_ASSERT_ERR);

	/* Test bad input string */
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test6_0(str, 'x', NULL, 0, BSTR_ASSERT_ERR);
		ret += test6_0(str, '\0', NULL, 0, BSTR_ASSERT_ERR);
	}

	unsigned char staticBuffers[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];


	static const char testChars[] = "x\n";

	/* Note: concatenating \0 is valid operation */
	for (int i = 0; i < (int)ARR_SIZE(testChars); ++i)
	{
		char ch = testChars[i];
		initGoodBstrings(&staticBuffers, &strings);

		unsigned char resultBuf[256];

		for (int j = 0; j < (int)TOTAL_GOOD_STRING_COUNT; ++j)
		{
			bstring* str = &strings[j];
			ASSERT(str->slen < 254);
			memcpy(resultBuf, str->data, str->slen);
			resultBuf[str->slen] = ch;
			resultBuf[str->slen + 1] = '\0';
			ret += test6_0(str, ch, resultBuf, str->slen + 1, BSTR_OK);
		}

		deinitGoodBstrings(&staticBuffers, &strings);
	}



	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}


static int test7x8_0 (
	const char * fnname, int (* fnptr) (const bstring *, const bstring *), 
	const bstring * b0, const bstring * b1, int res) 
{
	int rv, ret = 0;

	ret += (res != (rv = fnptr (b0, b1)));
	if (ret) {
		LOGF(eERROR, "  Expected: %s(%s) = %d", fnname, dumpBstring(b0), res);
		LOGF(eERROR, "       Got: %s(%s) = %d", fnname, dumpBstring(b0), rv);
	}
	return ret;
}

static int test7x8 (int test, const char * fnname, int (* fnptr) (const bstring *, const bstring *),
	                int retLT, int retGT, int retEQ) 
{
	int ret = 0;

	LOGF(eINFO, "TEST %d: int %s (const bstring* b0, const bstring* b1);", test, fnname);

	/* failing tests */
	ret += test7x8_0 (fnname, fnptr, NULL, NULL, BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		ret += test7x8_0(fnname, fnptr, str, NULL, BSTR_ASSERT_ERR);
		ret += test7x8_0(fnname, fnptr, NULL, str, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* badStr = &badConstBstrings[j];
			ret += test7x8_0(fnname, fnptr, str, badStr, BSTR_ASSERT_ERR);
			ret += test7x8_0(fnname, fnptr, badStr, str, BSTR_ASSERT_ERR);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* b0 = &badConstBstrings[i];
		ret += test7x8_0(fnname, fnptr, b0, NULL, BSTR_ASSERT_ERR);
		ret += test7x8_0(fnname, fnptr, NULL, b0, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test7x8_0(fnname, fnptr, b0, b1, BSTR_ASSERT_ERR);
		}
	}


	/* normal operation tests on all sorts of subranges */
	ret += test7x8_0 (fnname, fnptr, &emptyBstring, &emptyBstring, retEQ);
	ret += test7x8_0 (fnname, fnptr, &shortBstring, &emptyBstring, retGT);
	ret += test7x8_0 (fnname, fnptr, &emptyBstring, &shortBstring, retLT);
	ret += test7x8_0 (fnname, fnptr, &shortBstring, &shortBstring, retEQ);

	{
		bstring b = bdynfromstr(&shortBstring, 0);
		ret += test7x8_0(fnname, fnptr, &b, &shortBstring, retEQ);
		b.data[1]++;
		ret += test7x8_0 (fnname, fnptr, &b, &shortBstring, retGT);
		bdestroy (&b);
	}

	if (fnptr == biseq) {
		ret += test7x8_0 (fnname, fnptr, &shortBstring, &longBstring, retGT);
		ret += test7x8_0 (fnname, fnptr, &longBstring, &shortBstring, retLT);
	} else {
		ret += test7x8_0 (fnname, fnptr, &shortBstring, &longBstring, 'b'-'T');
		ret += test7x8_0 (fnname, fnptr, &longBstring, &shortBstring, 'T'-'b');
	}

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test7()
{
	return test7x8(7, "biseq", biseq, 0, 0, 1);
}

static int test8() 
{ 
	return test7x8(8, "bstrcmp", bstrcmp, -1, 1, 0);
}

static int test47_0 (const bstring* b, const unsigned char* blk, int len, int res) 
{
	int rv, ret = 0;

	ret += (res != (rv = biseqblk (b, blk, len)));
	if (ret) {
		LOGF(eERROR, "  Expected: biseqblk(%s, %s) = %d", dumpBstring(b), dumpCstring((const char*)blk), res);
		LOGF(eERROR, "       Got: biseqblk(%s, %s) = %d", dumpBstring(b), dumpCstring((const char*)blk),  rv);
	}
	return ret;
}

static int test47 (void) {
int ret = 0;

	LOGF(eINFO, "TEST 47: int biseqblk (const bstring* b, const void * blk, int len);");

	/* tests with NULL */
	ret += test47_0 (NULL, NULL, 0, BSTR_ASSERT_ERR);
	ret += test47_0 (&emptyBstring, NULL, 0, BSTR_ASSERT_ERR);
	ret += test47_0 (NULL, emptyBstring.data, 0, BSTR_ASSERT_ERR);
	ret += test47_0 (&shortBstring, NULL, shortBstring.slen, BSTR_ASSERT_ERR);
	ret += test47_0 (NULL, shortBstring.data, 0, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* bad = &badConstBstrings[i];
		ret += test47_0(bad, bad->data, bad->slen, BSTR_ASSERT_ERR);
		ret += test47_0(&shortBstring, bad->data, bad->slen, BSTR_ASSERT_ERR);
		ret += test47_0(bad, shortBstring.data, shortBstring.slen, BSTR_ASSERT_ERR);
	}

	/* normal operation tests on all sorts of subranges */
	ret += test47_0 (&emptyBstring, emptyBstring.data, emptyBstring.slen, 1);
	ret += test47_0 (&shortBstring, emptyBstring.data, emptyBstring.slen, 0);
	ret += test47_0 (&emptyBstring, shortBstring.data, shortBstring.slen, 0);
	ret += test47_0 (&shortBstring, shortBstring.data, shortBstring.slen, 1);

	{
		bstring b = bdynfromstr (&shortBstring, 0);
		b.data[1]++;
		ret += test47_0 (&b, shortBstring.data, shortBstring.slen, 0);
		bdestroy (&b);
	}
	ret += test47_0 (&shortBstring, longBstring.data, longBstring.slen, 0);
	ret += test47_0 (&longBstring, shortBstring.data, shortBstring.slen, 0);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test9_0 (const bstring* b0, const bstring* b1, int n, int res) 
{
	int rv, ret = 0;

	ret += (res != (rv = bstrncmp (b0, b1, n)));

	if (ret) {
		LOGF(eERROR, "  Expected: bstrncmp(%s, %s, %d) = %d", 
			dumpBstring(b0), dumpBstring(b1), n, res);
		LOGF(eERROR, "       Got: bstrncmp(%s, %s, %d) = %d", 
			dumpBstring(b0), dumpBstring(b1), n, rv);
	}
	return ret;
}

static int test9 (void) 
{
	int ret = 0;

	LOGF(eINFO, "TEST 9: int bstrncmp (const bstring* b0, const bstring* b1, int n);");

	/* tests with NULL */
	ret += test9_0 (NULL, NULL, 0, BSTR_ASSERT_ERR);
	ret += test9_0 (NULL, NULL, -1, BSTR_ASSERT_ERR);
	ret += test9_0 (NULL, NULL, 1, BSTR_ASSERT_ERR);
	ret += test9_0 (&emptyBstring, NULL, 0, BSTR_ASSERT_ERR);
	ret += test9_0 (NULL, &emptyBstring, 0, BSTR_ASSERT_ERR);
	ret += test9_0 (&emptyBstring, NULL, 1, BSTR_ASSERT_ERR);
	ret += test9_0 (NULL, &emptyBstring, 1, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* badStr = &badBstrings[i];
		ret += test9_0(badStr, badStr, 1, BSTR_ASSERT_ERR);
		ret += test9_0(&emptyBstring, badStr, 1, BSTR_ASSERT_ERR);
		ret += test9_0(badStr, &emptyBstring, 1, BSTR_ASSERT_ERR);
		ret += test9_0(&shortBstring, badStr, 1, BSTR_ASSERT_ERR);
		ret += test9_0(badStr, &shortBstring, 1, BSTR_ASSERT_ERR);
	}


	/* normal operation tests on all sorts of subranges */
	ret += test9_0 (&emptyBstring, &emptyBstring, -1, 0);
	ret += test9_0 (&emptyBstring, &emptyBstring, 0, 0);
	ret += test9_0 (&emptyBstring, &emptyBstring, 1, 0);
	ret += test9_0 (&shortBstring, &shortBstring, -1, 0);
	ret += test9_0 (&shortBstring, &shortBstring, 0, 0);
	ret += test9_0 (&shortBstring, &shortBstring, 1, 0);
	ret += test9_0 (&shortBstring, &shortBstring, 9, 0);
	ret += test9_0(&longBstring, &longBstring2, INT_MAX, ' ');
	ret += test9_0(&longBstring2, &longBstring, INT_MAX, -(int)' ');


	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		bstring copy = bdynfromstr(str, 0);
		ret += test9_0(str, &copy, 9, 0);
		if (copy.slen > 0 && strlen((char*)&str->data[0]) == (size_t)str->slen)
		{
			--copy.slen;
			ret += test9_0(str, &copy, INT_MAX, 1);
			ret += test9_0(&copy, str, INT_MAX, -1);
		}

		bdestroy(&copy);
	}

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test10_0 (bstring* b, int res, int nochange) {
	bstring sb = bconstfromliteral("<NULL>");
	int rv, x, ret = 0;

	if (b) sb = *b;
	rv = bdestroy (b);

	if (b != NULL) {
		if (rv >= 0)
			/* If the bdestroy was successful we have to assume
			   the contents were "changed" */
			x = 1;
		else
			x = memcmp (&sb, b, sizeof sb);
	} else x = !nochange;
	ret += (rv != res);
	ret += (!nochange) == (!x);
	if (ret) {
		LOGF(eERROR, "\t\tfailure(%d) res = %d nochange = %d, x = %d, sb.slen = %d, sb.mlen = %d, sb.data = %p", __LINE__, res, nochange, x, sb.slen, sb.mlen, sb.data);
	}
	return ret;
}

static int test10 (void) {
	int ret = 0;

	LOGF(eINFO, "TEST 10: int bdestroy (bstring* b);");
	/* tests with NULL */
	ret += test10_0 (NULL, BSTR_ASSERT_ERR, 1);

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test10_0(str, BSTR_ASSERT_ERR, 1);
	}

	/* 
	 * no need to test for good bdestroy, as it is tested by other tests 
	*/

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

int test11x12( int (*test_fn)(const bstring* s1, int pos, const bstring* s2, int res) )
{
	int ret = 0;

	ret += test_fn(NULL, 0, NULL, BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		ret += test_fn(str, 0, NULL, BSTR_ASSERT_ERR);
		ret += test_fn(NULL, 0, str, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* bad = &badConstBstrings[j];
			ret += test_fn(str, 0, bad, BSTR_ASSERT_ERR);
			ret += test_fn(bad, 0, str, BSTR_ASSERT_ERR);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* b0 = &badConstBstrings[i];
		ret += test_fn(b0, 0, NULL, BSTR_ASSERT_ERR);
		ret += test_fn(NULL, 0, b0, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test_fn(b0, 0, b1, BSTR_ASSERT_ERR);
		}
	}
	return ret;
}


static int test11_0 (const bstring* s1, int pos, const bstring* s2, int res) {
int rv, ret = 0;

	rv = binstr (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: binstr(%s, %d, %s) = ",
			dumpBstring(s1), pos, dumpBstring(s2));
		LOGF(eERROR, "            %d", res);
		LOGF(eERROR, "       Got: %d", rv);
	}
	return ret;
}

static int test11_1 (const bstring* s1, int pos, const bstring* s2, int res) {
int rv, ret = 0;

	rv = binstrcaseless (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: binstrcaseless(%s, %d, %s) = ",
			dumpBstring(s1), pos, dumpBstring(s2));
		LOGF(eERROR, "            %d", res);
		LOGF(eERROR, "       Got: %d", rv);
	}
	return ret;
}

static int test11 (void) {
	bstring b, c;
	int ret = 0;

	b = bempty();
	c = bempty();

	LOGF(eINFO, "TEST 11: int binstr (const bstring* s1, int pos, const bstring* s2);");
	ret += test11x12(test11_0);

	ret += test11_0 (&emptyBstring, 0, &emptyBstring, 0);
	ret += test11_0 (&emptyBstring, 1, &emptyBstring, BSTR_ERR);
	ret += test11_0 (&shortBstring, 1, &shortBstring, BSTR_ERR);
	ret += test11_0 (&shortBstring, 5, &emptyBstring, 5);
	ret += test11_0 (&shortBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test11_0 (&shortBstring, 0, &shortBstring, 0);
	ret += test11_0 (&shortBstring, 0, (bassign(&b, &shortBstring), &b), 0);
	ret += test11_0 (&shortBstring, 0, (bassignliteral(&b, "BOGUS"), &b), BSTR_ERR);
	ret += test11_0 (&longBstring, 0, &shortBstring, 10);
	ret += test11_0 (&longBstring, 20, &shortBstring, BSTR_ERR);
	ret += test11_0 (&shortBstring, 20, &emptyBstring, BSTR_ERR);
	ret += test11_0 (&shortBstring, 0, &emptyBstring, 0);
	ret += test11_0 (&shortBstring, 4, &emptyBstring, 4);


	ret += test11_0 ((bassignliteral(&c, "sssssssssap"), &c), 0, (bassignliteral(&b, "sap"), &b), 8);
	ret += test11_0 ((bassignliteral(&c, "sssssssssap"), &c), 3, (bassignliteral(&b, "sap"), &b), 8);
	ret += test11_0 ((bassignliteral(&c, "ssssssssssap"), &c), 3, (bassignliteral(&b, "sap"), &b), 9);
	ret += test11_0 ((bassignliteral(&c, "sssssssssap"), &c), 0, (bassignliteral(&b, "s"), &b), 0);
	ret += test11_0 ((bassignliteral(&c, "sssssssssap"), &c), 3, (bassignliteral(&b, "s"), &b), 3);
	ret += test11_0 ((bassignliteral(&c, "sssssssssap"), &c), 0, (bassignliteral(&b, "a"), &b), 9);
	ret += test11_0 ((bassignliteral(&c, "sssssssssap"), &c), 5, (bassignliteral(&b, "a"), &b), 9);
	ret += test11_0 ((bassignliteral(&c, "sasasasasap"), &c), 0, (bassignliteral(&b, "sap"), &b), 8);
	ret += test11_0 ((bassignliteral(&c, "ssasasasasap"), &c), 0, (bassignliteral(&b, "sap"), &b), 9);

	LOGF(eINFO, "TEST 11: int binstrcaseless (const bstring* s1, int pos, const bstring* s2);");
	ret += test11x12(test11_1);

	ret += test11_1 (&emptyBstring, 0, &emptyBstring, 0);
	ret += test11_1 (&emptyBstring, 1, &emptyBstring, BSTR_ERR);
	ret += test11_1 (&shortBstring, 1, &shortBstring, BSTR_ERR);
	ret += test11_1 (&shortBstring, 5, &emptyBstring, 5);
	ret += test11_1 (&shortBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test11_1 (&shortBstring, 0, &shortBstring, 0);
	ret += test11_1 (&shortBstring, 0, (bassign(&b, &shortBstring), &b), 0);
	ret += test11_1 (&shortBstring, 0, (bassignliteral(&b, "BOGUS"), &b), 0);
	ret += test11_1 (&longBstring, 0, &shortBstring, 10);
	ret += test11_1 (&longBstring, 20, &shortBstring, BSTR_ERR);
	ret += test11_1(&shortBstring, 20, &emptyBstring, BSTR_ERR);
	ret += test11_1(&shortBstring, 0, &emptyBstring, 0);
	ret += test11_1(&shortBstring, 4, &emptyBstring, 4);

	LOGF(eINFO, "\t# failures: %d", ret);

	bdestroy(&b);
	bdestroy(&c);

	return ret;
}

static int test12_0 (const bstring* s1, int pos, const bstring* s2, int res) {
int rv, ret = 0;

	rv = binstrr (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: binstrr(%s, %d, %s) = ",
			dumpBstring(s1), pos, dumpBstring(s2));
		LOGF(eERROR, "            %d", res);
		LOGF(eERROR, "       Got: %d", rv);
	}
	return ret;
}

static int test12_1 (const bstring* s1, int pos, const bstring* s2, int res) {
	int rv, ret = 0;

	rv = binstrrcaseless (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: binstrrcaseless(%s, %d, %s) = ",
			dumpBstring(s1), pos, dumpBstring(s2));
		LOGF(eERROR, "            %d", res);
		LOGF(eERROR, "       Got: %d", rv);
	}
	return ret;
}

static int test12 (void) {
	bstring b = bempty();
	int ret = 0;

	LOGF(eINFO, "TEST 12: int binstrr (const bstring* s1, int pos, const bstring* s2);");
	ret += test11x12(test12_0);

	ret += test12_0 (&emptyBstring, 0, &emptyBstring, 0);
	ret += test12_0 (&emptyBstring, 1, &emptyBstring, 0);
	ret += test12_0 (&shortBstring, 1, &shortBstring, 0);
	ret += test12_0 (&shortBstring, 5, &emptyBstring, 5);
	ret += test12_0 (&shortBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test12_0 (&shortBstring, 0, &shortBstring, 0);
	ret += test12_0 (&shortBstring, 0, (bassign(&b, &shortBstring), &b), 0);
	ret += test12_0 (&shortBstring, 0, (bassignliteral(&b, "BOGUS"), &b), BSTR_ERR);
	ret += test12_0 (&longBstring, 0, &shortBstring, BSTR_ERR);
	ret += test12_0 (&longBstring, 20, &shortBstring, 10);
	ret += test12_0 (&shortBstring, 20, &emptyBstring, shortBstring.slen);
	ret += test12_0 (&shortBstring, shortBstring.slen, &longBstring, BSTR_ERR);
	ret += test12_0 (&shortBstring, 0, &emptyBstring, 0);
	ret += test12_0 (&shortBstring, 4, &emptyBstring, 4);


	LOGF(eINFO, "TEST 12: int binstrrcaseless (const bstring* s1, int pos, const bstring* s2);");
	ret += test11x12(test12_1);


	ret += test12_1 (&emptyBstring, 0, &emptyBstring, 0);
	ret += test12_1 (&emptyBstring, 1, &emptyBstring, 0);
	ret += test12_1 (&shortBstring, 1, &shortBstring, 0);
	ret += test12_1 (&shortBstring, 5, &emptyBstring, 5);
	ret += test12_1 (&shortBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test12_1 (&shortBstring, 0, &shortBstring, 0);
	ret += test12_1 (&shortBstring, 0, (bassign(&b, &shortBstring), &b), 0);
	ret += test12_1 (&shortBstring, 0, (bassignliteral(&b, "BOGUS"), &b), 0);
	ret += test12_1 (&longBstring, 0, &shortBstring, BSTR_ERR);
	ret += test12_1 (&longBstring, 20, &shortBstring, 10);
	ret += test12_1 (&shortBstring, 20, &emptyBstring, shortBstring.slen);
	ret += test12_1 (&shortBstring, shortBstring.slen, &longBstring, BSTR_ERR);
	ret += test12_1 (&shortBstring, 0, &emptyBstring, 0);
	ret += test12_1 (&shortBstring, 4, &emptyBstring, 4);

	LOGF(eINFO, "\t# failures: %d", ret);
	bdestroy(&b);
	return ret;
}

static int test13_0 (const bstring* s1, int pos, const bstring* s2, int res) 
{
	int rv, ret = 0;

	rv = binchr (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: binchr(%s, %d, %s) = ",
			dumpBstring(s1), pos, dumpBstring(s2));
		LOGF(eERROR, "            %d", res);
		LOGF(eERROR, "       Got: %d", rv);
	}
	return ret;
}

static int test13 (void) {
	bstring b = bempty();
	int ret = 0;
	const bstring multipleOs = bconstfromliteral ("ooooo");
	const bstring beginAlphabet = bconstfromliteral("abcdefg");

	LOGF(eINFO, "TEST 13: int binchr (const bstring* s1, int pos, const bstring* s2);");
	ret += test13_0 (NULL, 0, NULL, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		ret += test13_0(str, 0, NULL, BSTR_ASSERT_ERR);
		ret += test13_0(str, 1, NULL, BSTR_ASSERT_ERR);
		ret += test13_0(str, -1, NULL, BSTR_ASSERT_ERR);
		ret += test13_0(NULL, 0, str, BSTR_ASSERT_ERR);
		ret += test13_0(NULL, 1, str, BSTR_ASSERT_ERR);
		ret += test13_0(NULL, -1, str, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* badStr = &badConstBstrings[j];
			ret += test13_0(str, 0, badStr, BSTR_ASSERT_ERR);
			ret += test13_0(str, 1, badStr, BSTR_ASSERT_ERR);
			ret += test13_0(str, -1, badStr, BSTR_ASSERT_ERR);
			ret += test13_0(badStr, 0, str, BSTR_ASSERT_ERR);
			ret += test13_0(badStr, 1, str, BSTR_ASSERT_ERR);
			ret += test13_0(badStr, -1, str, BSTR_ASSERT_ERR);
		}
	}

	ret += test13_0 (&emptyBstring, 0, &emptyBstring, BSTR_ERR);
	ret += test13_0 (&shortBstring, 0, &emptyBstring, BSTR_ERR);
	ret += test13_0 (&shortBstring,  0, &shortBstring, 0);
	ret += test13_0 (&shortBstring,  0, &multipleOs, 1);
	ret += test13_0 (&shortBstring, 0, (bassign(&b, &shortBstring), &b), 0);
	ret += test13_0 (&shortBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test13_0 (&shortBstring, 10, &shortBstring, BSTR_ERR);
	ret += test13_0 (&shortBstring, 1, &shortBstring, 1);
	ret += test13_0 (&emptyBstring, 0, &shortBstring, BSTR_ERR);
	ret += test13_0 (&longBstring, 0, &shortBstring, 3);
	ret += test13_0 (&longBstring, 10, &shortBstring, 10);
	ret += test13_0(&multipleOs, 0, &beginAlphabet, BSTR_ERR);
	ret += test13_0(&multipleOs, 3, &beginAlphabet, BSTR_ERR);
	ret += test13_0(&multipleOs, -1, &beginAlphabet, BSTR_ASSERT_ERR);



	bdestroy(&b);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test14_0 (const bstring* s1, int pos, const bstring* s2, int res) {
int rv, ret = 0;

	rv = binchrr (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: binchrr(%s, %d, %s) = ",
			dumpBstring(s1), pos, dumpBstring(s2));
		LOGF(eERROR, "            %d", res);
		LOGF(eERROR, "       Got: %d", rv);
	}
	return ret;
}

static int test14 (void) {
	bstring b = bempty();
	int ret = 0;
	const bstring multipleOs = bconstfromliteral("ooooo");
	const bstring beginAlphabet = bconstfromliteral("abcdefg");

	LOGF(eINFO, "TEST 14: int binchrr (const bstring* s1, int pos, const bstring* s2);");
	ret += test14_0(NULL, 0, NULL, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		ret += test14_0(str, 0, NULL, BSTR_ASSERT_ERR);
		ret += test14_0(str, 1, NULL, BSTR_ASSERT_ERR);
		ret += test14_0(str, -1, NULL, BSTR_ASSERT_ERR);
		ret += test14_0(NULL, 0, str, BSTR_ASSERT_ERR);
		ret += test14_0(NULL, 1, str, BSTR_ASSERT_ERR);
		ret += test14_0(NULL, -1, str, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* badStr = &badConstBstrings[j];
			ret += test14_0(str, 0, badStr, BSTR_ASSERT_ERR);
			ret += test14_0(str, 1, badStr, BSTR_ASSERT_ERR);
			ret += test14_0(str, -1, badStr, BSTR_ASSERT_ERR);
			ret += test14_0(badStr, 0, str, BSTR_ASSERT_ERR);
			ret += test14_0(badStr, 1, str, BSTR_ASSERT_ERR);
			ret += test14_0(badStr, -1, str, BSTR_ASSERT_ERR);
		}
	}

	ret += test14_0 (&shortBstring,  0, &shortBstring, 0);
	ret += test14_0 (&shortBstring, 0, (bassign(&b, &shortBstring), &b), 0);
	ret += test14_0 (&shortBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test14_0 (&shortBstring, 5, &shortBstring, 4);
	ret += test14_0 (&shortBstring, 4, &shortBstring, 4);
	ret += test14_0 (&shortBstring, 1, &shortBstring, 1);
	ret += test14_0 (&emptyBstring, 0, &shortBstring, BSTR_ERR);
	ret += test14_0 (&longBstring, 0, &shortBstring, BSTR_ERR);
	ret += test14_0 (&longBstring, 10, &shortBstring, 10);
	ret += test14_0(&multipleOs, 0, &beginAlphabet, BSTR_ERR);
	ret += test14_0(&multipleOs, 3, &beginAlphabet, BSTR_ERR);
	ret += test14_0(&multipleOs, -1, &beginAlphabet, BSTR_ASSERT_ERR);
	bdestroy(&b);
	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test15_0 (bstring* b0, int pos, const bstring* b1, unsigned char fill, const bstring* res, int resRv) {
	int rv, ret = 0;

	bstring copy = { 0,0,NULL };
	if (bisvalid(b0))
		copy = bdynfromstr(b0, 0);
	else if (b0)
		copy = *b0;

	rv = bsetstr(b0, pos, b1, fill);

	ret += rv != resRv;
	if (bconstisvalid(res))
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b0);

		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	}
	else
		ret += bisvalid(b0);

	if (ret)
	{
		LOGF(eERROR, "  Expected: bsetstr(%s, %d, %s, %c) = ", 
			dumpBstring(&copy), pos, dumpBstring(b1), fill );
		LOGF(eERROR, "            %d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: %d, %s", rv, dumpBstring(b0));
	}

	if (bisvalid(&copy))
		bdestroy(&copy);

	return ret;
}

static int test15(void) {
	int ret = 0;
	LOGF(eINFO, "TEST 15: int bsetstr (bstring* b0, int pos, const bstring* b1, unsigned char fill);");
	/* tests with NULL */

	ret += test15_0(NULL, 0, NULL, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);

	for (int i = 0; i <= (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = NULL;
		if (i != ARR_SIZE(goodConstBstrings))
			b1 = &goodConstBstrings[i];

		ret += test15_0(NULL, 0, b1, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badBstrings); ++j)
		{
			bstring* b0 = (bstring*)&badBstrings[j];
			ret += test15_0(b0, 0, b1, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b0 = (bstring*)&pseudoGoodBstrings[i];
		/* Note: NULL is a valid b1 input for bsetstr */
		ret += test15_0(b0, -1, NULL, (unsigned char) '?', b0, BSTR_ASSERT_ERR);
		ret += test15_0(b0, INT_MIN, NULL, (unsigned char) '?', b0, BSTR_ASSERT_ERR);

		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test15_0(b0, 0, b1, (unsigned char) '?', b0, BSTR_ASSERT_ERR);
		}
	}




	/* normal operation tests */
	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	/* No changes */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* b0 = &strings[i];
		bstring copy = bdynfromstr(b0, 0);
		ret += test15_0(b0, b0->slen, &emptyBstring, '?', &copy, BSTR_OK);
		bdestroy(&copy);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* cat(b0,b1)*/
	for (int i = 0; i <= (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = NULL;
		if (i != ARR_SIZE(goodConstBstrings))
			b1 = &goodConstBstrings[i];

		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			bstring res = bdynfromstr(b0, 256);
			bconcat(&res, b1);
			ret += test15_0(b0, b0->slen, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}
	
	/* same as bassign(b0,b1) */
	for (int i = 0; i <= (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = NULL;
		if (i != ARR_SIZE(goodConstBstrings))
			b1 = &goodConstBstrings[i];

		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			bstring res = bempty();
			if (b1)
				res = bdynfromstr(b1, 0);

			ret += test15_0(b0, 0, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}

	/* Same as concat(b0,"?????",b1) */
	for (int i = 0; i <= (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = NULL;
		if (i != ARR_SIZE(goodConstBstrings))
			b1 = &goodConstBstrings[i];

		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			bstring res = bdynfromstr(b0, 256);
			bcatliteral(&res, "?????");
			bconcat(&res, b1);
			ret += test15_0(b0, b0->slen + 5, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}

	/* replace in the middle */
	for (int i = 0; i <= (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = NULL;
		if (i != ARR_SIZE(goodConstBstrings))
			b1 = &goodConstBstrings[i];

		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			int pos = 2;
			if (b0->slen <= pos)
				pos = 0;

			bstring res = bdynallocfromcstr("", 256);
			bcatblk(&res, b0->data, pos);
			bconcat(&res, b1);

			ret += test15_0(b0, pos, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}


	/* Aliasing */
	initGoodBstrings(&staticBufs, &strings);
	for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
	{
		bstring* b0 = &strings[j];
		int pos = 2;
		if (b0->slen < pos)
			pos = 0;

		bstring res = bdynallocfromcstr("", 256);
		bcatblk(&res, b0->data, pos);
		bconcat(&res, b0);

		ret += test15_0(b0, pos, b0, '?', &res, BSTR_OK);
		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test16_0 (bstring* b0, int pos, const bstring* b1, unsigned char fill, const bstring* res, int rvResult) {
	int rv, ret = 0;

	rv = binsert(b0, pos, b1, fill);
	ret += rv != rvResult;
	if (bisvalid(res))
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b0);
		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	}
	else
		ret += bisvalid(b0);

	if (ret) {
		LOGF(eERROR, "\t\tfailure(%d) = %d (res = %p", __LINE__, ret, res);
		if (res) LOGF(eINFO, " = \"%s\"", res);
		LOGF(eINFO, ")\n");
	}
	return ret;
}

//static int test16_1 (void) {
//	bstring* b0 = bfromStatic ("aaaaabbbbb");
//	bstring b1;
//	int res, ret = 0;
//
//	bmid2tbstr (b1, b0, 4, 4);
//	b0->slen = 6;
//
//	LOGF(eINFO, ".\tbinsert (%s, 2, %s, '?') = ", dumpBstring (b0), dumpBstring (&b1));
//	res = binsert (b0, 2, &b1, '?');
//	LOGF(eINFO, "%s (Alias test)\n", dumpBstring (b0));
//
//	ret += (res != 0);
//	ret += !biseqStatic(b0, "aaabbbaaab");
//
//	return ret;
//}

static int test16 (void) {
int ret = 0;
	LOGF(eINFO, "TEST 16: int binsert (bstring* b0, int pos, const bstring* b1, unsigned char fill);");
	/* tests with NULL */
	ret += test16_0 (NULL, 0, NULL, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
	/* NULL b0 tests*/
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = &goodConstBstrings[i];
		ret += test16_0(NULL, 0, b1, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
	}
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* b1 = &badConstBstrings[i];
		ret += test16_0(NULL, 0, b1, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
	}
	/* Bad b0 tests*/
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* b0 = (bstring*)&badBstrings[i];
		ret += test16_0(b0, 0, NULL, '?', NULL, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test16_0(b0, 0, b1, '?', NULL, BSTR_ASSERT_ERR);
		}
		for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
		{
			const bstring* b1 = &goodConstBstrings[j];
			ret += test16_0(b0, 0, b1, '?', NULL, BSTR_ASSERT_ERR);
		}
	}
	/* Bad b1/pos tests */
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b0 = (bstring*)&pseudoGoodBstrings[i];
		/* Don't need to copy b0, because b0 is in const memory */
		ret += test16_0(b0, 0, NULL, '?', b0, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test16_0(b0, 0, b1, '?', b0, BSTR_ASSERT_ERR);
		}
		for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
		{
			const bstring* b1 = &goodConstBstrings[j];
			ret += test16_0(b0, -1, b1, '?', b0, BSTR_ASSERT_ERR);
		}
	}

	///* normal operation tests */
	unsigned char staticBuf[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	/* concat b0,b1 */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = &goodConstBstrings[i];
		initGoodBstrings(&staticBuf, &strings);
		
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			bstring res = bdynfromstr(b0, 256);
			int rv = bconcat(&res, b1);
			ASSERT(rv == BSTR_OK);
			ret += test16_0(b0, b0->slen, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}

		deinitGoodBstrings(&staticBuf, &strings);
	}
	/* concat b1, b0 */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = &goodConstBstrings[i];
		initGoodBstrings(&staticBuf, &strings);

		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			bstring res = bdynfromstr(b1, 256);
			int rv = bconcat(&res, b0);
			ASSERT(rv == BSTR_OK);
			ret += test16_0(b0, 0, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}

		deinitGoodBstrings(&staticBuf, &strings);
	}
	/* concat b0,"?????",b1 */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b1 = &goodConstBstrings[i];
		initGoodBstrings(&staticBuf, &strings);

		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			int rv;
			bstring* b0 = &strings[j];
			bstring res = bdynfromstr(b0, 256);
			rv = bcatliteral(&res, "?????");
			ASSERT(rv == BSTR_OK);
			rv = bconcat(&res, b1);
			ASSERT(rv == BSTR_OK);
			ret += test16_0(b0, b0->slen + 5, b1, '?', &res, BSTR_OK);
			bdestroy(&res);
		}

		deinitGoodBstrings(&staticBuf, &strings);
	}
	
	/* Insert self at 0 */
	initGoodBstrings(&staticBuf, &strings);

	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		int rv;
		bstring* b0 = &strings[i];
		bstring res = bdynfromstr(b0, 256);
		rv = bconcat(&res, b0);
		ASSERT(rv == BSTR_OK);
		ret += test16_0(b0, 0, b0, '?', &res, BSTR_OK);
		bdestroy(&res);
	}

	deinitGoodBstrings(&staticBuf, &strings);

	/* Insert self at end */
	initGoodBstrings(&staticBuf, &strings);

	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		int rv;
		bstring* b0 = &strings[i];
		bstring res = bdynfromstr(b0, 256);
		rv = bconcat(&res, b0);
		ASSERT(rv == BSTR_OK);
		ret += test16_0(b0, b0->slen, b0, '?', &res, BSTR_OK);
		bdestroy(&res);
	}

	deinitGoodBstrings(&staticBuf, &strings);

	/* Insert self after end */
	initGoodBstrings(&staticBuf, &strings);

	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		int rv;
		bstring* b0 = &strings[i];
		bstring res = bdynfromstr(b0, 256);
		rv = bcatliteral(&res, "?????");
		ASSERT(rv == BSTR_OK);
		rv = bconcat(&res, b0);
		ASSERT(rv == BSTR_OK);
		ret += test16_0(b0, b0->slen + 5, b0, '?', &res, BSTR_OK);
		bdestroy(&res);
	}

	deinitGoodBstrings(&staticBuf, &strings);

	/* Insert self in the middle */
	initGoodBstrings(&staticBuf, &strings);

	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		int rv;
		bstring* b0 = &strings[i];
		if (b0->slen < 2)
			continue;
		bstring res = bdynfromstr(b0, 256);
		res.slen = 2;
		rv = bconcat(&res, b0);
		ASSERT(rv == BSTR_OK);
		rv = bcatblk(&res, b0->data + 2, b0->slen - 2);
		ASSERT(rv == BSTR_OK);
		ret += test16_0(b0, 2, b0, '?', &res, BSTR_OK);
		bdestroy(&res);
	}

	deinitGoodBstrings(&staticBuf, &strings);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test17_0 (bstring* s1, int pos, int len, const bstring* res, int rvRes) {
	int rv, ret = 0;

	rv = bdelete(s1, pos, len);
	ret += rv != rvRes;
	if (bisvalid(res))
	{
		/* bdelete won't write '\0' if pos >= s1->slen or len == 0 */
		if (rv == BSTR_OK && pos < s1->slen && len != 0)
			ret += !correctBstring(s1);
		ret += !bisvalid(s1) || biseq(s1, res) != 1;
	}
	else
	{
		ret += bisvalid(s1);
	}

	if (ret) {
		LOGF(eINFO, "\t\tfailure(%d) = %d (res = %p", __LINE__, ret, res);
		if (res) LOGF(eINFO, " = \"%s\"", res);
		LOGF(eINFO, ")\n");
	}
	return ret;
}

static int test17 (void) 
{
	int ret = 0;
	LOGF(eINFO, "TEST 17: int bdelete (bstring* s1, int pos, int len);");
	/* tests with NULL */
	ret += test17_0 (NULL, 0, 0, NULL, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test17_0(str, 0, 0, NULL, BSTR_ASSERT_ERR);
		ret += test17_0(str, 0, -1, NULL, BSTR_ASSERT_ERR);
		ret += test17_0(str, -5, 4, NULL, BSTR_ASSERT_ERR);
		ret += test17_0(str, -5, INT_MAX, NULL, BSTR_ASSERT_ERR);
		ret += test17_0(str, INT_MAX, INT_MAX, NULL, BSTR_ASSERT_ERR);
	}
	/* tests with invalid/no effect inputs */
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test17_0(str, 0, -1, str, BSTR_ASSERT_ERR);
		ret += test17_0(str, -5, 4, str, BSTR_ASSERT_ERR);
		ret += test17_0(str, 0, 0, str, BSTR_OK);
		ret += test17_0(str, str->slen, 1, str, BSTR_OK);
	}

	/* normal operation tests */
	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];


	/* delete from end */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		if (str->slen == 0)
			continue;
		bstring res = bempty();
		ASSERT(str->slen > 4);
		int rv = bassignmidstr(&res, str, 0, 4);
		ASSERT(rv == BSTR_OK);
		ret += test17_0(str, 4, INT_MAX, &res, BSTR_OK);

		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* delete from begin */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		if (str->slen == 0)
			continue;
		bstring res = bempty();
		ASSERT(str->slen > 4);
		int rv = bassignmidstr(&res, str, 2, INT_MAX);
		ASSERT(rv == BSTR_OK);
		ret += test17_0(str, 0, 2, &res, BSTR_OK);

		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* delete from middle */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		if (str->slen == 0)
			continue;
		bstring res = bempty();
		ASSERT(str->slen > 4);
		int rv = bassignmidstr(&res, str, 0, 1);
		ASSERT(rv == BSTR_OK);
		rv = bcatblk(&res, str->data + 3, str->slen - 3);
		ret += test17_0(str, 1, 2, &res, BSTR_OK);

		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);


	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test18_0 (bstring* b, int len, int res, int mlen) 
{
	int ret = 0;
	int rv;
	int ol = 0;

	if (b) ol = bmlen(b);
	rv = balloc (b, len);

	if (b != NULL && b->data != NULL && b->slen >=0 && ol > bmlen(b)) {
		LOGF(eERROR, "\t\tfailure(%d) oldmlen = %d, newmlen %d\n", __LINE__, ol, b->mlen);
		ret++;
	}

	if (rv != res) {
		LOGF(eERROR, "\t\tfailure(%d) res = %d\n", __LINE__, res);
		ret++;
	}
	else if (rv == BSTR_OK)
	{
		ret += !bisvalid(b);
	}
	if (bisvalid(b) && (mlen > bmlen(b) || bmlen(b) == 0)) {
		LOGF(eINFO, "\t\tfailure(%d) b->mlen = %d mlen = %d\n", __LINE__, b->mlen, mlen);
		ret++;
	}
	return ret;
}

static int test18_1 (bstring* b, int len, int res, int mlen) 
{
	int ret = 0;
	int rv;
	int ol = 0;

	if (b) ol = bmlen(b);

	rv = ballocmin (b, len);

	if (bisvalid(b) && bmlen(b) != mlen) 
	{
		ret++;
	}

	if (rv != res) {
		ret++;
	}
	else if (rv == BSTR_OK)
	{
		ret += !bisvalid(b);
	}

	if (ret)
	{
		LOGF(eERROR, "  Expected: ballocmin(%d) = rv: %d, mlen: %d", len, res, mlen);
		LOGF(eERROR, "       Got: ballocmin(%d) = rv: %d, mlen: %d", len, rv, bmlen(b));
		LOGF(eERROR, "  Old mlen: %d", ol);
	}

	return ret;
}


static int test18 (void) {
	int ret = 0;

	LOGF(eINFO, "TEST 18: int balloc (bstring* s, int len);");
	/* tests with NULL */
	ret += test18_0 (NULL, 2, BSTR_ASSERT_ERR, 0);
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test18_0(str, 2, BSTR_ASSERT_ERR, 0);
	}
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test18_0(str, -10, BSTR_ASSERT_ERR, bmlen(str));
	}


	/* normal operation tests */

	/* No realloc */
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test18_0(str, 1, BSTR_OK, 1);
	}

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	/* Guarantee realloc */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		ret += test18_0(str, 500, BSTR_OK, 500);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* realloc to mlen(check that bstring is correct) */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		ret += test18_0(str, bmlen(str), BSTR_OK, bmlen(str));
	}
	deinitGoodBstrings(&staticBufs, &strings);

	LOGF(eINFO, "TEST 18: int ballocmin (bstring* s, int len);");
	/* tests with NULL */
	ret += test18_1(NULL, 2, BSTR_ASSERT_ERR, 0);
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test18_1(str, 2, BSTR_ASSERT_ERR, 0);
	}
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test18_1(str, -10, BSTR_ASSERT_ERR, bmlen(str));
	}

	/* normal operation tests */

	/* Realloc to str size + 1 */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int mlen = str->slen + 1;
		/* Static string would realloc only if mlen is less than requested */
		if (!bownsdata(str))
			mlen = bmlen(str) > mlen ? bmlen(str) : mlen;
		ret += test18_1(str, str->slen + 1, BSTR_OK, mlen);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* realloc to str->slen - 1*/
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int len = str->slen - 1;
		int mlen = str->slen + 1;
		int rv = len < 0 ? BSTR_ASSERT_ERR : BSTR_OK;
		if (rv == BSTR_ASSERT_ERR)
			mlen = bmlen(str);
		else if (!bownsdata(str))
			mlen = mlen > bmlen(str) ? mlen : bmlen(str);
		ret += test18_1(str, len, rv, mlen);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* Realloc to str bmlen + 100 */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int mlen = bmlen(str) + 100;
		ret += test18_1(str, mlen, BSTR_OK, mlen);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	//bdestroy (b);
	LOGF(eINFO, "\t# failures: %d", ret);

	return ret;
}

static int test19_0 (bstring* b, int len, const bstring* res, int resRv) {
	int rv, ret = 0;
	bstring copy = bdynfromstr(b, 0);

	rv = bpattern(b, len);

	ret += rv != resRv;
	if (rv == BSTR_OK)
		ret += !correctBstring(b);

	if (bconstisvalid(res))
		ret += !bisvalid(b) || biseq(b, res) != 1;
	else
		ret += bisvalid(b);

	if (ret)
	{
		LOGF(eERROR, "  Expected: bpattern(%s, %d) = %d, %s", dumpBstring(&copy), len, resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: bpattern(%s, %d) = %d, %s", dumpBstring(&copy), len, rv, dumpBstring(b));
	}

	bdestroy(&copy);

	return ret;
}

static int test19 (void) {
int ret = 0;

	LOGF(eINFO, "TEST 19: int bpattern (bstring* b, int len);");
	/* tests with NULL */
	ret += test19_0 (NULL, 0, NULL, BSTR_ASSERT_ERR);
	ret += test19_0(NULL, 5, NULL, BSTR_ASSERT_ERR);
	ret += test19_0(NULL, -5, NULL, BSTR_ASSERT_ERR);

	/* invalid string */
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test19_0(str, 0, NULL, BSTR_ASSERT_ERR);
		ret += test19_0(str, 5, NULL, BSTR_ASSERT_ERR);
		ret += test19_0(str, -5, NULL, BSTR_ASSERT_ERR);
	}

	/* invalid len */
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test19_0(str, -1, str, BSTR_ASSERT_ERR);
		ret += test19_0(str, -5, str, BSTR_ASSERT_ERR);
	}


	/* normal operation tests */

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];
	
	initGoodBstrings(&staticBufs, &strings);
	/* zero size pattern */
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int rv = biseqliteral(str, "") == 1 ? BSTR_ERR : BSTR_OK;
		ret += test19_0(str, 0, &emptyBstring, rv);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	/* first character */
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int rv = BSTR_OK;
		bstring res = bdynfromstr(str, 0);
		res.slen = 1;
		if (biseqliteral(str, "") == 1)
		{
			rv = BSTR_ERR;
			res.slen = 0;
		}
		ret += test19_0(str, 1, &res, rv);
		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	/* 5 repetitions */
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int rv = biseqliteral(str, "") == 1 ? BSTR_ERR : BSTR_OK;
		bstring res = bempty();

		for (int j = 0; j < 5; ++j)
		{
			bconcat(&res, str);
		}
		ret += test19_0(str, str->slen * 5, &res, rv);
		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	/* 5 repetitions and exclude last 2 symbols */
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		int rv = biseqliteral(str, "") == 1 ? BSTR_ASSERT_ERR : BSTR_OK;
		bstring res = bempty();
		int len = str->slen * 5 - 2;

		for (int j = 0; j < 5; ++j)
		{
			bconcat(&res, str);
		}
		res.slen = len > 0 ? len : 0;

		ret += test19_0(str, len, &res, rv);
		bdestroy(&res);
	}
	deinitGoodBstrings(&staticBufs, &strings);


	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

#define NULL_BSTRING ((bstring*)NULL)
#define NULL_CSTRING ((char*)NULL)

// Calls formatting function and checks result
#define test20_0x1_0(fn, b, fmt, res, resRv, ...)			\
    int localRet = 0;										\
	{														\
		int rv = fn(b, fmt, __VA_ARGS__);					\
															\
		localRet += rv != resRv;							\
		if (rv == BSTR_OK)									\
			localRet += !correctBstring(b);					\
															\
		if (bisvalid(res))									\
			localRet += !bisvalid(b) || biseq(b, res) != 1;	\
		else												\
			localRet += bisvalid(b);						\
															\
		if (localRet)										\
		{													\
			LOGF(eERROR, "  Expected: bformat(%s, ...) = % d, %s", printablecstr(fmt), resRv, dumpBstring(res));	\
			LOGF(eERROR, "       Got: bformat(%s, ...) = % d, %s", printablecstr(fmt), rv,    dumpBstring(b));		\
		}												\
														\
	}													\
	ret += localRet;

#define STRINGIFY(x) #x

#define test20_0x1_1(fn, b, fmt, resRv, ...)				                            \
	if ((resRv) == BSTR_ASSERT_ERR)							                            \
	{														                            \
		test20_0x1_0(fn, b, fmt, b, resRv, __VA_ARGS__);	                            \
	}														                            \
	else													                            \
	{														                            \
		char buf[512] = "";									                            \
		char* pBegin = &buf[0];								                            \
		size_t size = sizeof(buf);							                            \
		size_t slen = 0;									                            \
		if (strcmp(STRINGIFY(fn), "bformata") == 0)			                            \
		{													                            \
			ASSERT((b)->slen >= 0 && (b)->slen < (int)size);                            \
			memcpy(pBegin, (b)->data, (b)->slen);			                            \
			pBegin += (b)->slen;							                            \
			size -= (b)->slen;								                            \
			slen += (b)->slen;								                            \
		}													                            \
        ASSERT(fmt != NULL);                                                            \
        /*fmt + !fmt is made for warning supression for GCC on Ubuntu*/                 \
		int rv0 = snprintf(pBegin, size, fmt + !fmt, __VA_ARGS__);	                    \
		ASSERT(rv0 >= 0 && rv0 < (int)size);						                    \
		bstring res = {(int)sizeof(buf), (int)(slen += rv0), (unsigned char*)&buf[0] };	\
		test20_0x1_0(fn, b, fmt, (&res), resRv, __VA_ARGS__);	                        \
	}

/* 
 * Note: these macros require at least one arg, 
 * so pass 0 in the end, because it won't affect outcome 
 */
#define test20_0(b, fmt, resRv, ...) test20_0x1_1(bformat, b, fmt, resRv, __VA_ARGS__)
#define test20_1(b, fmt, resRv, ...) test20_0x1_1(bformata, b, fmt, resRv, __VA_ARGS__)


static int test20 (void) {
	int ret = 0;


	#if !defined (BSTRLIB_NOVSNP)
	LOGF(eINFO, "TEST 20: int bformat (bstring* b, const char * fmt, ...);");

	static const char* simpleFormats[] =
	{
		"", 
		"aaaaaaa",
		"%%d %%s",
		"%%i %%s"
	};

	test20_0(NULL_BSTRING, NULL_CSTRING, BSTR_ASSERT_ERR, 0);

	for (int i = 0; i < (int)ARR_SIZE(simpleFormats); ++i)
	{
		const char* fmt = simpleFormats[i];
		test20_0(NULL_BSTRING, fmt, BSTR_ASSERT_ERR, 0);
	}

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		for (int j = 0; j < (int)ARR_SIZE(simpleFormats); ++j)
		{
			const char* fmt = simpleFormats[j];
			test20_0(str, fmt, BSTR_ASSERT_ERR, 0);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		test20_0(str, NULL_CSTRING, BSTR_ASSERT_ERR, 0);
	}

	/* Check that aliasing leads to error */
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		test20_0(str, "%s" , BSTR_ASSERT_ERR, str->data);
		test20_0(str, "%10d%s", BSTR_ASSERT_ERR, 4, str->data);
		test20_0(str, "%10d%1s", BSTR_ASSERT_ERR, 4, str->data);
	}

	bstring emptyStr = bempty();
	test20_0(&emptyStr, "%s", BSTR_OK, gEmptyStringBuffer);

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

#define test20_0_all_strings(fmt, ...)								\
	{																\
		initGoodBstrings(&staticBufs, &strings);					\
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)					\
		{															\
			test20_0( (&strings[j]), fmt, BSTR_OK, __VA_ARGS__);	\
		}															\
		deinitGoodBstrings(&staticBufs, &strings);					\
	}

	for (int i = 0; i < (int)ARR_SIZE(simpleFormats); ++i)
	{
		const char* fmt = simpleFormats[i];
		test20_0_all_strings(fmt, 0);
	}

	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i)
	{
		const char* cstr = cStrings[i];
		test20_0_all_strings("%s", cstr);
		test20_0_all_strings("%s%s%s", cstr,cstr,cstr);
	}

	test20_0_all_strings("%d %d %d", (int)6, (int)INT_MAX / 2, (int)INT_MAX / 1024);
	test20_0_all_strings("%d %d %d", (int)-6, (int)INT_MIN / 2, (int)INT_MIN / 1024);
	test20_0_all_strings("%10d %-d %+d", (int)6, (int)INT_MAX / 2, (int)INT_MAX / 1024);
	test20_0_all_strings("%10d %-d %+d", (int)-6, (int)INT_MIN / 2, (int)INT_MIN / 1024);

#undef test20_0_all_strings

	LOGF(eINFO, "\t# failures: %d", ret);


	
	LOGF(eINFO, "TEST 20: int bformata (bstring* b, const char * fmt, ...);");

	test20_1(NULL_BSTRING, NULL_CSTRING, BSTR_ASSERT_ERR, 0);

	for (int i = 0; i < (int)ARR_SIZE(simpleFormats); ++i)
	{
		const char* fmt = simpleFormats[i];
		test20_1(NULL_BSTRING, fmt, BSTR_ASSERT_ERR, 0);
	}

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		for (int j = 0; j < (int)ARR_SIZE(simpleFormats); ++j)
		{
			const char* fmt = simpleFormats[j];
			test20_1(str, fmt, BSTR_ASSERT_ERR, 0);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		test20_1(str, NULL_CSTRING, BSTR_ASSERT_ERR, 0);
	}

	/* Check that aliasing leads to error */
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		test20_1(str, "%s", BSTR_ASSERT_ERR, str->data);
		test20_1(str, "%10d%s", BSTR_ASSERT_ERR, 4, str->data);
		test20_1(str, "%10d%1s", BSTR_ASSERT_ERR, 4, str->data);
	}

	emptyStr = bempty();
	test20_1(&emptyStr, "%s", BSTR_OK, gEmptyStringBuffer);

#define test20_1_all_strings(fmt, ...)								\
	{																\
		initGoodBstrings(&staticBufs, &strings);					\
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)					\
		{															\
			test20_1( (&strings[j]), fmt, BSTR_OK, __VA_ARGS__);	\
		}															\
		deinitGoodBstrings(&staticBufs, &strings);					\
	}

	for (int i0 = 0; i0 < (int)ARR_SIZE(simpleFormats); ++i0)
	{
		const char* fmt = simpleFormats[i0];
		test20_1_all_strings(fmt, 0);
	}

	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i)
	{
		const char* cstr = cStrings[i];
		test20_1_all_strings("%s", cstr);
		test20_1_all_strings("%s%s%s", cstr, cstr, cstr);
	}

	test20_1_all_strings("%d %d %d", (int)6, (int)INT_MAX / 2, (int)INT_MAX / 1024);
	test20_1_all_strings("%d %d %d", (int)-6, (int)INT_MIN / 2, (int)INT_MIN / 1024);
	test20_1_all_strings("%10d %-d %+d", (int)6, (int)INT_MAX / 2, (int)INT_MAX / 1024);
	test20_1_all_strings("%10d %-d %+d", (int)-6, (int)INT_MIN / 2, (int)INT_MIN / 1024);

#undef test20_1_all_strings
	LOGF(eINFO, "\t# failures: %d", ret);

#endif

	return ret;
}


static int test21_0 (const bstring* b, char splitChar, const bstring res[], int resCount) {
	int ret = 0;

	bstring* split = bsplit(b, splitChar);

	if ((res == NULL && split != NULL) ||
		(res != NULL && split == NULL))
		ret = 1;

	if (arrlen(split) != resCount)
		ret = 1;

	if (ret == 0)
	{
		for (int i = 0; i < resCount; ++i)
		{
			ret += !biseq(&res[i], &split[i]);
		}
	}

	if (ret)
	{
		const char* dumpStr = dumpBstring(b);
		const char* dumpExpArr = dumpBstringArr(res, resCount);
		const char* dumpResArr = dumpBstringArr(split, (int)arrlen(split));

		LOGF(eERROR, "  Expected: bsplit(%s, '%c') = bstring[%d]:%s", dumpStr, splitChar, resCount, dumpExpArr);
		LOGF(eERROR, "       Got: bsplit(%s, '%c') = bstring[%d]:%s", dumpStr, splitChar, (int)arrlen(split), dumpResArr);
	}
	for (ptrdiff_t i = 0; i < arrlen(split); ++i)
		bdestroy(&split[i]);
	
	arrfree(split);

	return ret;
}

static int test21_1(const bstring* b, const bstring* splitStr, const bstring res[], int resCount) {
	int ret = 0;

	bstring* split = bsplitstr(b, splitStr);

	if ((res == NULL && split != NULL) ||
		(res != NULL && split == NULL))
		ret = 1;

	if (arrlen(split) != resCount)
		ret = 1;

	if (ret == 0)
	{
		for (int i = 0; i < resCount; ++i)
		{
			ret += !biseq(&res[i], &split[i]);
		}
	}

	if (ret)
	{
		const char* dumpStr = dumpBstring(b);
		const char* dumpSplit = dumpBstring(splitStr);
		const char* dumpExpArr = dumpBstringArr(res, resCount);
		const char* dumpResArr = dumpBstringArr(split, (int)arrlen(split));

		LOGF(eERROR, "  Expected: bsplitstr(%s, %s) = bstring[%d]:%s", dumpStr, dumpSplit, resCount, dumpExpArr);
		LOGF(eERROR, "       Got: bsplitstr(%s, %s) = bstring[%d]:%s", dumpStr, dumpSplit, (int)arrlen(split), dumpResArr);
	}
	for (ptrdiff_t i = 0; i < arrlen(split); ++i)
		bdestroy(&split[i]);

	arrfree(split);

	return ret;
}

static int test21_2(const bstring* b, const bstring* splitChars, const bstring res[], int resCount) {
	int ret = 0;

	bstring* split = bsplits(b, splitChars);

	if ((res == NULL && split != NULL) ||
		(res != NULL && split == NULL))
		ret = 1;

	if (arrlen(split) != resCount)
		ret = 1;

	if (ret == 0)
	{
		for (int i = 0; i < resCount; ++i)
		{
			ret += !biseq(&res[i], &split[i]);
		}
	}

	if (ret)
	{
		const char* dumpStr = dumpBstring(b);
		const char* dumpSplit = dumpBstring(splitChars);
		const char* dumpExpArr = dumpBstringArr(res, resCount);
		const char* dumpResArr = dumpBstringArr(split, (int)arrlen(split));

		LOGF(eERROR, "  Expected: bsplits(%s, %s) = bstring[%d]:%s", dumpStr, dumpSplit, resCount, dumpExpArr);
		LOGF(eERROR, "       Got: bsplits(%s, %s) = bstring[%d]:%s", dumpStr, dumpSplit, (int)arrlen(split), dumpResArr);
	}
	for (ptrdiff_t i = 0; i < arrlen(split); ++i)
		bdestroy(&split[i]);

	arrfree(split);

	return ret;
}

static int test21(void) 
{
	const bstring is = bconstfromliteral ("is");
	const bstring ng = bconstfromliteral("ng");
	const bstring commas = bconstfromliteral(",,,,");
	int ret = 0;

	LOGF(eINFO, "TEST 21: bstring* bsplit (const bstring* str, unsigned char splitChar);");
	{
		/* tests with NULL */
		ret += test21_0(NULL, (char) '?', NULL, 0);
		for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
		{
			ret += test21_0(&badConstBstrings[i], (char) '?', NULL, 0);
		}


		///* normal operation tests */
		const bstring shortRes0[] = 
		{
			bconstfromliteral("b"),
			bconstfromliteral("gus")
		};
		const bstring shortRes1[] = 
		{
			bconstfromliteral("bogu"),
			bempty()
		};
		const bstring shortRes2[] = 
		{
			bempty(),
			bconstfromliteral("ogus"),
		};
		const bstring longRes[] = 
		{
			bconstfromliteral("This is a b"),
			bconstfromliteral("gus but reas"),
			bconstfromliteral("nably l"),
			bconstfromliteral("ng string.  Just l"),
			bconstfromliteral("ng en"),
			bconstfromliteral("ugh t"),
			bconstfromliteral(" cause s"),
			bconstfromliteral("me mall"),
			bconstfromliteral("cing."),
		};
		const bstring commasRes[5] = {
			bempty(), bempty(), bempty(), bempty(), bempty()
		};

		for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
			ret += test21_0(&goodConstBstrings[i], (char) '?', &goodConstBstrings[i], 1);

		ret += test21_0(&shortBstring, (char) 'o', shortRes0, ARR_SIZE(shortRes0));
		ret += test21_0(&shortBstring, (char) 's', shortRes1, ARR_SIZE(shortRes1));
		ret += test21_0(&shortBstring, (char) 'b', shortRes2, ARR_SIZE(shortRes2));
		ret += test21_0(&longBstring, (char) 'o', longRes, ARR_SIZE(longRes));
		ret += test21_0(&commas, (char) ',', commasRes, ARR_SIZE(commasRes));
	}

	LOGF(eINFO, "TEST 21: bstring* bsplitstr (bstring* str, const bstring* splitStr);");
	{
		ret += test21_1(NULL, NULL, NULL, 0);

		for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
		{
			ret += test21_1(&badConstBstrings[i], NULL, NULL, 0);
			ret += test21_1(NULL, &badConstBstrings[i], NULL, 0);
			for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
				ret += test21_1(&badConstBstrings[i], &badConstBstrings[j], NULL, 0);

			for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
			{
				ret += test21_1(&badConstBstrings[i], &goodConstBstrings[j], NULL, 0);
				ret += test21_1(&goodConstBstrings[j], &badConstBstrings[i], NULL, 0);
			}
		}

		/* normal operation tests */
		static const bstring shortRes[] =
		{
			bconstfromliteral("b"),
			bconstfromliteral("o"),
			bconstfromliteral("g"),
			bconstfromliteral("u"),
			bconstfromliteral("s")
		};
		static const bstring longIsRes[] = 
		{
			bconstfromliteral("Th"),
			bconstfromliteral(" "),
			bconstfromliteral(" a bogus but reasonably long string.  Just long enough to cause some mallocing.")
		};
		static const bstring longNgRes[] =
		{
			bconstfromliteral("This is a bogus but reasonably lo"),
			bconstfromliteral(" stri"),
			bconstfromliteral(".  Just lo"),
			bconstfromliteral(" enough to cause some malloci"),
			bconstfromliteral(".")
		};

		for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
		{
			const bstring split = bconstfromliteral("?");
			const bstring splitSelfRes[] = { bempty(), bempty() };
			ret += test21_1(&goodConstBstrings[i], &split, &goodConstBstrings[i], 1);
			// Splitting empty bstring always gives empty bstring
			if (biseq(&emptyBstring, &goodConstBstrings[i]))
				ret += test21_1(&goodConstBstrings[i], &goodConstBstrings[i], &emptyBstring, 1);
			// Spliting by self should give 2 empty strings
			else
				ret += test21_1(&goodConstBstrings[i], &goodConstBstrings[i], splitSelfRes, ARR_SIZE(splitSelfRes));
		}
			

		ret += test21_1(&shortBstring, &emptyBstring, shortRes, ARR_SIZE(shortRes));
		ret += test21_1 (&longBstring, &is, longIsRes, ARR_SIZE(longIsRes));
		ret += test21_1 (&longBstring, &ng, longNgRes, ARR_SIZE(longNgRes));
	}

	LOGF(eINFO, "TEST 21: bstring* bsplits (bstring* str, const bstring* splitChars);");
	{
		ret += test21_2(NULL, NULL, NULL, 0);

		for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
		{
			ret += test21_2(&badConstBstrings[i], NULL, NULL, 0);
			ret += test21_2(NULL, &badConstBstrings[i], NULL, 0);
			for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
				ret += test21_2(&badConstBstrings[i], &badConstBstrings[j], NULL, 0);

			for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
			{
				ret += test21_2(&badConstBstrings[i], &goodConstBstrings[j], NULL, 0);
				ret += test21_2(&goodConstBstrings[j], &badConstBstrings[i], NULL, 0);
			}
		}

		/* normal operation tests */
		for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
		{
			const bstring split = bconstfromliteral("?");
			bstring splitSelfRes[128];
			for (int j = 0; j < (int)ARR_SIZE(splitSelfRes); ++j)
				splitSelfRes[j] = bempty();

			ret += test21_2(&goodConstBstrings[i], &split, &goodConstBstrings[i], 1);
			// Splitting empty bstring always gives singular empty bstring
			if (biseq(&emptyBstring, &goodConstBstrings[i]))
				ret += test21_2(&goodConstBstrings[i], &goodConstBstrings[i], &emptyBstring, 1);
			// Spliting by self should give the number of characters in the string + 1(before begin should be added)
			else
			{
				ASSERT(goodConstBstrings[i].slen + 1 < (int)ARR_SIZE(splitSelfRes));
				ret += test21_2(&goodConstBstrings[i], &goodConstBstrings[i], splitSelfRes, goodConstBstrings[i].slen + 1);
			}
		}

		static const bstring o=bconstfromliteral("o");
		static const bstring s= bconstfromliteral("s");
		static const bstring b= bconstfromliteral("b");
		static const bstring bs= bconstfromliteral("bs");
		static const bstring uo= bconstfromliteral("uo");

		static const bstring shortRes0[] = 
		{
			bconstfromliteral(SHORT_STRING)
		};
		static const bstring shortRes1[] =
		{
			bconstfromliteral("b"),
			bconstfromliteral("gus")
		};
		static const bstring shortRes2[] =
		{
			bconstfromliteral("bogu"), 
			bconstfromliteral("")
		};
		static const bstring shortRes3[] =
		{
			bconstfromliteral(""), 
			bconstfromliteral("ogus")
		};
		static const bstring shortRes4[] =
		{
			bconstfromliteral(""), 
			bconstfromliteral("ogu"), 
			bconstfromliteral("")
		};
		static const bstring shortRes5[] =
		{
			bconstfromliteral("b"), 
			bconstfromliteral("g"), 
			bconstfromliteral("s")
		};
		static const bstring longRes[] = 
		{
			bconstfromliteral("This is a b"), 
			bconstfromliteral("gus but reas"), 
			bconstfromliteral("nably l"), 
			bconstfromliteral("ng string.  Just l"), 
			bconstfromliteral("ng en"), 
			bconstfromliteral("ugh t"), 
			bconstfromliteral(" cause s"), 
			bconstfromliteral("me mall"), 
			bconstfromliteral("cing.")
		};

		ret += test21_2(&emptyBstring, &o, &emptyBstring, 1);
		ret += test21_2(&emptyBstring, &uo, &emptyBstring, 1);
		ret += test21_2(&shortBstring, &emptyBstring, shortRes0, ARR_SIZE(shortRes0));
		ret += test21_2(&shortBstring, &o, shortRes1, ARR_SIZE(shortRes1));
		ret += test21_2(&shortBstring, &s, shortRes2, ARR_SIZE(shortRes2));
		ret += test21_2(&shortBstring, &b, shortRes3, ARR_SIZE(shortRes3));
		ret += test21_2(&shortBstring, &bs, shortRes4, ARR_SIZE(shortRes4));
		ret += test21_2(&shortBstring, &uo, shortRes5, ARR_SIZE(shortRes5));
		ret += test21_2(&longBstring, &o, longRes, ARR_SIZE(longRes));;
	}

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test22_0(bstring* out, const bstring inputs[], int count, const bstring* sep, const bstring* res)
{
	int ret = 0;
	int rv = bjoin(out, inputs, count, sep);

	if (bconstisvalid(res))
	{
		if (rv == BSTR_OK && bisvalid(out))
			ret += !biseq(res, out) || !biscstr(out);
		else
			++ret;
	}
	else
		ret += rv == BSTR_OK;
	
	if (ret != 0)
	{

		LOGF(eERROR, "  Expected: bjoin(bstring[%d]:%s) = %s", count, dumpBstringArr(inputs, count), dumpBstring(res));
		LOGF(eERROR, "       Got: bjoin(bstring[%d]:%s) = %s", count, dumpBstringArr(inputs, count), dumpBstring(out));
	}
	return ret;
}

static int test22(void)
{
	int ret = 0;
	LOGF(eINFO, "TEST 22: int bjoin (bstring* out, const bstring inputs[], int count, const bstring* sep);");

	ret += test22_0(NULL, NULL, 0, NULL, NULL);
	
	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test22_0(str, NULL, 1, &goodConstBstrings[0], NULL);


		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			ret += test22_0(str, NULL, 0, &badConstBstrings[j], NULL);
			ret += test22_0(str, NULL, 1, &badConstBstrings[j], NULL);
			ret += test22_0(str, goodConstBstrings, ARR_SIZE(goodConstBstrings), &badConstBstrings[j], NULL);
		}
	}

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		ret += test22_0(&strings[i], NULL, 0, NULL, &emptyBstring);
	
	deinitGoodBstrings(&staticBufs, &strings);


	for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
	{
		initGoodBstrings(&staticBufs, &strings);
		for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
			ret += test22_0(&strings[i], &goodConstBstrings[j], 1, NULL, &goodConstBstrings[j]);

		deinitGoodBstrings(&staticBufs, &strings);
	}

	const bstring inputs[] =
	{
		emptyBstring,
		shortBstring,
		longBstring
	};

	const bstring output0 = bconstfromliteral(EMPTY_STRING SHORT_STRING LONG_STRING);
	const bstring commaSeparator = bconstfromliteral(",");
	const bstring output1 = bconstfromliteral(EMPTY_STRING "," SHORT_STRING "," LONG_STRING);
	const bstring complexSeparator = bconstfromliteral("abcde");
	const bstring output2 = bconstfromliteral(EMPTY_STRING "abcde" SHORT_STRING "abcde" LONG_STRING);

	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		ret += test22_0(&strings[i], inputs, ARR_SIZE(inputs), NULL, &output0);
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		ret += test22_0(&strings[i], inputs, ARR_SIZE(inputs), &emptyBstring, &output0);
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		ret += test22_0(&strings[i], inputs, ARR_SIZE(inputs), &commaSeparator, &output1);
	deinitGoodBstrings(&staticBufs, &strings);

	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		ret += test22_0(&strings[i], inputs, ARR_SIZE(inputs), &complexSeparator, &output2);
	deinitGoodBstrings(&staticBufs, &strings);

	/* aliasing tests (run only if previous tests succeded) */
	if (ret == 0)
	{
		bstring output = bempty();

		/* aliasing separator */
		initGoodBstrings(&staticBufs, &strings);
		for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		{
			int localRet = bjoin(&output, inputs, ARR_SIZE(inputs), &strings[i]);
			ASSERT(localRet == BSTR_OK);
			ret += test22_0(&strings[i], inputs, ARR_SIZE(inputs), &strings[i], &output);
		}
		deinitGoodBstrings(&staticBufs, &strings);


		/* aliasing inputs */
		bstring aliasingInputs[3];
		initGoodBstrings(&staticBufs, &strings);
		for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
		{
			for (int j = 0; j < 3; ++j)
				aliasingInputs[j] = strings[i];

			int localRet = bjoin(&output, aliasingInputs, ARR_SIZE(inputs), &commaSeparator);
			ASSERT(localRet == BSTR_OK);
			ret += test22_0(&strings[i], aliasingInputs, ARR_SIZE(inputs), &commaSeparator, &output);
		}
		deinitGoodBstrings(&staticBufs, &strings);

		bdestroy(&output);
	}

	
	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test24_0 (const bstring* s1, int pos, const bstring* s2, int res) 
{
	int rv, ret = 0;

	rv = bninchr(s1, pos, s2);
	ret += rv != res;
	if (ret)
	{
		LOGF(eERROR, "  Expected: bninchr(%s, % d, %s) = % d", dumpBstring(s1), pos, dumpBstring(s2), res);
		LOGF(eERROR, "       Got: bninchr(%s, % d, %s) = % d", dumpBstring(s1), pos, dumpBstring(s2), rv);
	}
	return ret;
}

static int test24 (void) {
	int ret = 0;
	bstring b = bempty();

	LOGF(eINFO, "TEST 24: int bninchr (const bstring* s1, int pos, const bstring* s2);");
	ret += test24_0 (NULL, 0, NULL, BSTR_ASSERT_ERR);
	ret += test24_0 (&emptyBstring, 0, NULL, BSTR_ASSERT_ERR);
	ret += test24_0 (NULL, 0, &emptyBstring, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		ret += test24_0(&shortBstring, 3, &badConstBstrings[i], BSTR_ASSERT_ERR);
		ret += test24_0(&badConstBstrings[i], 3, &shortBstring, BSTR_ASSERT_ERR);
	}


	ret += test24_0 (&emptyBstring, 0, &emptyBstring, BSTR_ERR);
	ret += test24_0 (&shortBstring, 0, &emptyBstring, 0);
	ret += test24_0 (&shortBstring,  0, &shortBstring, BSTR_ERR);
	ret += test24_0 (&shortBstring,  1, &shortBstring, BSTR_ERR);
	ret += test24_0 (&longBstring, 3, &shortBstring, 4);
	ret += test24_0 (&longBstring, 3, &(b = bdynfromstr(&shortBstring, 0)), 4);
	bdestroy (&b);
	ret += test24_0 (&longBstring, -1, &shortBstring, BSTR_ASSERT_ERR);
	ret += test24_0 (&longBstring, 1000, &shortBstring, BSTR_ERR);

	ret += test24_0 (&emptyBstring, 0, &shortBstring, BSTR_ERR);

	ret += test24_0 (&longBstring, 0, &shortBstring, 0);
	ret += test24_0 (&longBstring, 10, &shortBstring, 15);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test25_0 (const bstring* s1, int pos, const bstring* s2, int res) 
{
	int rv, ret = 0;
	rv = bninchrr (s1, pos, s2);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: bninchrr(%s, % d, %s) = % d", dumpBstring(s1), pos, dumpBstring(s2), res);
		LOGF(eERROR, "       Got: bninchrr(%s, % d, %s) = % d", dumpBstring(s1), pos, dumpBstring(s2), rv);
	}
	return ret;
}

static int test25 (void) 
{
	bstring b;
	int ret = 0;

	LOGF(eINFO, "TEST 25: int bninchrr (const bstring* s1, int pos, const bstring* s2);");
	ret += test25_0(NULL, 0, NULL, BSTR_ASSERT_ERR);
	ret += test25_0(&emptyBstring, 0, NULL, BSTR_ASSERT_ERR);
	ret += test25_0(NULL, 0, &emptyBstring, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		ret += test25_0(&shortBstring, 3, &badConstBstrings[i], BSTR_ASSERT_ERR);
		ret += test25_0(&badConstBstrings[i], 3, &shortBstring, BSTR_ASSERT_ERR);
	}


	ret += test25_0 (&shortBstring,  0, &shortBstring, BSTR_ERR);
	ret += test25_0 (&shortBstring,  4, &shortBstring, BSTR_ERR);
	ret += test25_0 (&shortBstring, 4, &emptyBstring, 4);
	ret += test25_0 (&shortBstring, 0, &emptyBstring, 0);
	ret += test25_0(&shortBstring, shortBstring.slen, &emptyBstring, shortBstring.slen - 1);
	ret += test25_0 (&longBstring, 10, &shortBstring, 9);
	ret += test25_0 (&longBstring, 10, &( b = bdynfromstr(&shortBstring, 0)), 9);
	bdestroy (&b);
	ret += test25_0 (&emptyBstring, 0, &shortBstring, BSTR_ERR);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test26_0 (bstring* b0, int pos, int len, const bstring* b1, unsigned char fill, const bstring* res, int resRv) {
	bstring copy = { 0,0,NULL };
	int rv, ret = 0;
	if (!bisvalid(b0) && b0)
		copy = *b0;
	else if (b0)
		copy = bdynfromstr(b0, 0);

	rv = breplace(b0, pos, len, b1, fill);
	ret += rv != resRv;

	if (bconstisvalid(res))
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b0);

		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	}
	else
		ret += bisvalid(b0);


	if (ret) 
	{
		LOGF(eERROR, "  Expected: breplace(%s,%d,%d,%s,%c) = ", 
			dumpBstring(&copy), pos, len, dumpBstring(b1), fill);
		LOGF(eERROR, "            %d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: %d, %s", rv, dumpBstring(b0));
	}
	
	if (bisvalid(&copy))
		bdestroy(&copy);

	return ret;
}

static bstring* bassignblkaux(bstring* out, const void* blk, int len)
{
	int ret = bassignblk(out, blk, len);
	return ret == BSTR_OK ? out : NULL;
}

#define bassignaux(b, str) bassignblkaux(b, ("" str ""), sizeof(str) - 1 )

static int test26 (void) 
{
	int ret = 0;
	LOGF(eINFO, "TEST 26: int breplace (bstring* b0, int pos, int len, const bstring* b1, unsigned char fill);");
	/* tests with NULL */
	ret += test26_0 (NULL, 0, 0, NULL, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
	ret += test26_0(NULL, 0, 0, &emptyBstring, (unsigned char) '?', NULL, BSTR_ASSERT_ERR);
	
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* b0 = (bstring*)&badBstrings[i];

		for (int j = 0; j < (int)ARR_SIZE(goodConstBstrings); ++j)
		{
			const bstring* b1 = &goodConstBstrings[j];
			ret += test26_0(b0, 0, 1, b1, '?', NULL, BSTR_ASSERT_ERR);
			ASSERT(!ret);
		}
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test26_0(b0, 0, 1, b1, '?', NULL, BSTR_ASSERT_ERR);
			ASSERT(!ret);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b0 = (bstring*)&pseudoGoodBstrings[i];
		ret += test26_0(b0, 0, 1, NULL, '?', b0, BSTR_ASSERT_ERR);

		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* b1 = &badConstBstrings[j];
			ret += test26_0(b0, 0, 1, b1, '?', b0, BSTR_ASSERT_ERR);
		}
	}


	///* normal operation tests */
	bstring res = bempty();
	
	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];
	
	/* No changes */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		bassign(&res, str);
		ret += test26_0(str, 0, 0, &emptyBstring, '?', &res, BSTR_OK);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* Insert after end */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b2 = &goodConstBstrings[i];
		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b1 = &strings[j];
			bassign(&res, b1);
			bcatliteral(&res, "?????");
			bconcat(&res, b2);
			ret += test26_0(b1, b1->slen + 5, INT_MAX, b2, '?', &res, BSTR_OK);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}

	/* Replace whole string */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b2 = &goodConstBstrings[i];
		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b1 = &strings[j];

			ret += test26_0(b1, 0, INT_MAX, b2, '?', b2, BSTR_OK);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}

	/* Replace whole string after 2 */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b2 = &goodConstBstrings[i];
		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b1 = &strings[j];
			bassignliteral(&res, "");
			if (b1->slen >= 2)
				bcatblk(&res, b1->data, 2);
			else
				bcatliteral(&res, "??");

			bconcat(&res, b2);

			ret += test26_0(b1, 2, INT_MAX, b2, '?', &res, BSTR_OK);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}

	/* Insert at 2 */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b2 = &goodConstBstrings[i];
		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b1 = &strings[j];
			bassignliteral(&res, "");
			if (b1->slen >= 2)
				bcatblk(&res, b1->data, 2);
			else
				bcatliteral(&res, "??");

			bconcat(&res, b2);

			if (b1->slen > 2)
				bcatblk(&res, b1->data + 2, b1->slen - 2);
			ret += test26_0(b1, 2, 0, b2, '?', &res, BSTR_OK);
		}

		deinitGoodBstrings(&staticBufs, &strings);
	}


	/* Insert at 2 aliasing */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* b1 = &strings[i];
		bassignliteral(&res, "");
		if (b1->slen >= 2)
			bcatblk(&res, b1->data, 2);
		else
			bcatliteral(&res, "??");

		bconcat(&res, b1);

		if (b1->slen > 2)
			bcatblk(&res, b1->data + 2, b1->slen - 2);
		ret += test26_0(b1, 2, 0, b1, '?', &res, BSTR_OK);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	/* Replace [2,4] */
	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b2 = &goodConstBstrings[i];
		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b1 = &strings[j];
			bassignliteral(&res, "");
			if (b1->slen >= 2)
				bcatblk(&res, b1->data, 2);
			else
				bcatliteral(&res, "??");

			bconcat(&res, b2);

			if (b1->slen > 4)
				bcatblk(&res, b1->data + 4, b1->slen - 4);
			ret += test26_0(b1, 2, 2, b2, '?', &res, BSTR_OK);
		}

		deinitGoodBstrings(&staticBufs, &strings);
	}


	/* Replace [2,4] aliasing */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* b1 = &strings[i];
		bassignliteral(&res, "");
		if (b1->slen >= 2)
			bcatblk(&res, b1->data, 2);
		else
			bcatliteral(&res, "??");

		bconcat(&res, b1);

		if (b1->slen > 4)
			bcatblk(&res, b1->data + 4, b1->slen - 4);
		ret += test26_0(b1, 2, 2, b1, '?', &res, BSTR_OK);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	bdestroy(&res);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}


static int test28_0 (const bstring* s1, int c, int res) 
{
	int rv, ret = 0;

	rv = bstrchr (s1, c);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: bstrchr(%s, '%c'(0x%2x)) = ", dumpBstring(s1), c, (int)c);
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test28_1 (const bstring* s1, int c, int res) 
{
	int rv, ret = 0;

	rv = bstrrchr (s1, c);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: bstrrchr(%s, '%c'(0x%2x)) = ", dumpBstring(s1), c, c);
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test28_2 (const bstring* s1, int c, int pos, int res) 
{
	int rv, ret = 0;

	rv = bstrchrp (s1, c, pos);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: bstrchrp(%s, (0x%2x), %d) = ", dumpBstring(s1), c, pos);
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test28_3 (const bstring* s1, int c, int pos, int res) 
{
	int rv, ret = 0;

	rv = bstrrchrp (s1, c, pos);
	ret += (rv != res);
	if (ret) {
		LOGF(eERROR, "  Expected: bstrrchrp(%s, '%c'(0x%2x), %d) = ", dumpBstring(s1), c, c, pos);
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test28 (void) {
	int ret = 0;

	LOGF(eINFO, "TEST 28: int bstrchr (const bstring* s1, int c);");
	ret += test28_0 (NULL, 0, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test28_0(str, 'b', BSTR_ASSERT_ERR);
	}

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		for (int ch = 0; ch < UCHAR_MAX; ++ch)
		{
			const void* ptr = memchr(str->data, ch, str->slen);
			int res = ptr ? (int)((unsigned char*)ptr - str->data) : BSTR_ERR;
			ret += test28_0(str, ch, res);
		}
	}

	LOGF(eINFO, "TEST 28: int bstrrchr (const bstring* s1, int c);");
	ret += test28_1(NULL, 0, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test28_1(str, 'b', BSTR_ASSERT_ERR);
	}

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		for (int ch = 0; ch < UCHAR_MAX; ++ch)
		{
			const unsigned char* ptr = str->data;
			int len = str->slen;
			int res = BSTR_ERR;
			ptr = (const unsigned char*)memchr(ptr, ch, len);
            while (ptr)
			{
				res = (int)(ptr - str->data);
				len = str->slen - res - 1;
				++ptr;
                ptr = (const unsigned char*)memchr(ptr, ch, len);
			}

			ret += test28_1(str, ch, res);
		}
	}

	LOGF(eINFO, "TEST 28: int bstrchrp (const bstring* s1, int c, int pos);");
	ret += test28_2 (NULL, 0, 0, BSTR_ASSERT_ERR);
	ret += test28_2(NULL, 'b', 0, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test28_2(str, 'b', 0, BSTR_ASSERT_ERR);
		ret += test28_2(str, 'b', -1, BSTR_ASSERT_ERR);
		ret += test28_2(str, 'b', 1, BSTR_ASSERT_ERR);
	}


	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		for (int ch = 0; ch < UCHAR_MAX; ++ch)
		{
			ret += test28_2(str, ch, -1, BSTR_ASSERT_ERR);
			ret += test28_2(str, ch, INT_MIN, BSTR_ASSERT_ERR);
			ret += test28_2(str, ch, str->slen, BSTR_ERR);
			ret += test28_2(str, ch, str->slen + 1, BSTR_ASSERT_ERR);
			ret += test28_2(str, ch, INT_MAX, BSTR_ASSERT_ERR);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		for (int ch = 0; ch < UCHAR_MAX; ++ch)
		{
			for (int j = 0; j <= str->slen; ++j)
			{
				const void* ptr = memchr(str->data + j, ch, str->slen - j);
				int res = ptr ? (int)((unsigned char*)ptr - str->data) : BSTR_ERR;
				ret += test28_2(str, ch, j, res);
			}
		}
	}


	LOGF(eINFO, "TEST 28: int bstrrchrp (const bstring* s1, int c, int pos);");
	ret += test28_3(NULL, 0, 0, BSTR_ASSERT_ERR);
	ret += test28_3(NULL, 'b', 0, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test28_3(str, 'b', 0, BSTR_ASSERT_ERR);
		ret += test28_3(str, 'b', -1, BSTR_ASSERT_ERR);
		ret += test28_3(str, 'b', 1, BSTR_ASSERT_ERR);
	}


	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* str = &goodConstBstrings[i];
		for (int ch = 0; ch < UCHAR_MAX; ++ch)
		{
			ret += test28_3(str, ch, -1, BSTR_ASSERT_ERR);
			ret += test28_3(str, ch, INT_MIN, BSTR_ASSERT_ERR);
			ret += test28_3(str, ch, str->slen + 1, BSTR_ASSERT_ERR);
			ret += test28_3(str, ch, INT_MAX, BSTR_ASSERT_ERR);
		}
	}



	ret += test28_3 (&emptyBstring, 0, 0, BSTR_ERR);
	ret += test28_3 (&shortBstring, 0, 0, BSTR_ERR);
	ret += test28_3 (&shortBstring, 'b', 0, 0);
	ret += test28_3 (&shortBstring, 'b', shortBstring.slen - 1, 0);
	ret += test28_3 (&shortBstring, 's', shortBstring.slen - 1, 4);
	ret += test28_3(&shortBstring, 'b', shortBstring.slen, 0);
	ret += test28_3(&shortBstring, 's', shortBstring.slen, 4);
	ret += test28_3 (&shortBstring, 's', 0, BSTR_ERR);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test29_0 (bstring* b0, const char * s, const bstring * res, int resRv) {
	int rv, ret = 0;
	bstring copy = { 0,0,NULL };
	if (bisvalid(b0))
		copy = bdynfromstr(b0, 0);
	else if (b0)
		copy = *b0;

	rv = bcatcstr(b0, s);
	ret += rv != resRv;

	if (bconstisvalid(res))
	{
		if (rv == BSTR_OK)
			ret += !correctBstring(b0);

		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	}
	else
		ret += bisvalid(b0);

	if (ret) 
	{
		LOGF(eERROR, "  Expected: bcatcstr(%s, %s) = ", dumpBstring(&copy), printablecstr(s));
		LOGF(eERROR, "            % d, %s", resRv, res);
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b0));
	}
	if (bisvalid(&copy))
		bdestroy(&copy);

	return ret;
}

static int test29 (void) {
int ret = 0;

	LOGF(eINFO, "TEST 29: int bcatcstr (bstring* b0, const char * s);");

	/* tests with NULL */
	ret += test29_0 (NULL, NULL, NULL, BSTR_ASSERT_ERR);
	ret += test29_0 (NULL, "", NULL, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* b0 = (bstring*)&badBstrings[i];
		ret += test29_0(b0, "", NULL, BSTR_ASSERT_ERR);
		ret += test29_0(b0, NULL, NULL, BSTR_ASSERT_ERR);
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b0 = (bstring*)&pseudoGoodBstrings[i];
		ret += test29_0(b0, NULL, b0, BSTR_ASSERT_ERR);
		/* Doesn't handle aliasing */
		ret += test29_0(b0, (const char*)b0->data, b0, BSTR_ASSERT_ERR);
	}


	/* normal operation tests on all sorts of subranges */
	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	for (int i = 0; i < (int)ARR_SIZE(cStrings); ++i)
	{
		const char* cstr = cStrings[i];


		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* str = &strings[j];
			bstring res = bdynfromstr(str, 256);
			
			bcatblk(&res, cstr, (int)strlen(cstr));
			ret += test29_0(str, cstr, &res, BSTR_OK);

			bdestroy(&res);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}


	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}


/* bcatblk is tested with bconcat and bcatcstr */
//static int test30_0 (bstring* b0, const unsigned char * s, int len, const char * res) 
//{
//	bstring* b2;
//	int rv, ret = 0;
//
//	if (b0 != NULL && b0->data != NULL && b0->slen >= 0) {
//		b2 = bstrcpy (b0);
//		bwriteprotect (*b2);
//
//		LOGF(eINFO, ".\tbcatblk (%s, ", dumpBstring (b2));
//
//		rv = bcatblk (b2, s, len);
//		ret += (rv == 0);
//		if (!biseq (b0, b2)) ret++;
//
//		LOGF(eINFO, "%p) = %s\n", s, dumpBstring (b2));
//
//		bwriteallow (*b2);
//
//		LOGF(eINFO, ".\tbcatblk (%s, ", dumpBstring (b2));
//
//		rv = bcatblk (b2, s, len);
//
//		LOGF(eINFO, "%p) = %s\n", s, dumpBstring (b2));
//
//		if (s) {
//			if (len >= 0) ret += (b2->slen != b0->slen + len);
//			else ret += (b2->slen != b0->slen);
//		}
//		ret += ((0 != rv) && (s != NULL && len >= 0)) || ((0 == rv) && (s == NULL || len < 0));
//		ret += (res == NULL) || ((int) strlen (res) != b2->slen)
//		       || (0 != memcmp (b2->data, res, b2->slen));
//		ret += b2->data[b2->slen] != '\0';
//		bdestroy (b2);
//	} else {
//		ret += (BSTR_ASSERT_ERR != (rv = bcatblk (b0, s, len)));
//		LOGF(eINFO, ".\tbcatblk (%s, %p, %d) = %d\n", dumpBstring (b0), s, len, rv);
//	}
//
//	if (ret) {
//		LOGF(eINFO, "\t\tfailure(%d) = %d (res = %p", __LINE__, ret, res);
//		if (res) LOGF(eINFO, " = \"%s\"", res);
//		LOGF(eINFO, ")\n");
//	}
//	return ret;
//}
//
//static int test30 (void) 
//{
//	int ret = 0;
//
//	LOGF(eINFO, "TEST: int bcatblk (bstring* b0, const char * s);\n");
//
//	/* tests with NULL */
//	ret += test30_0 (NULL, NULL, 0, NULL);
//	ret += test30_0 (NULL, (unsigned char *) "", 0, NULL);
//	ret += test30_0 (&emptyBstring, NULL, 0, "");
//	ret += test30_0 (&emptyBstring, NULL, -1, "");
//	ret += test30_0 (&badBstring1, NULL, 0, NULL);
//	ret += test30_0 (&badBstring2, NULL, 0, NULL);
//
//	/* normal operation tests on all sorts of subranges */
//	ret += test30_0 (&emptyBstring, (unsigned char *) "", -1, "");
//	ret += test30_0 (&emptyBstring, (unsigned char *) "", 0, "");
//	ret += test30_0 (&emptyBstring, (unsigned char *) "bogus", 5, "bogus");
//	ret += test30_0 (&shortBstring, (unsigned char *) "", 0, "bogus");
//	ret += test30_0 (&shortBstring, (unsigned char *) "bogus", 5, "bogusbogus");
//	ret += test30_0 (&shortBstring, (unsigned char *) "bogus", -1, "bogus");
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}


#ifdef ENABLE_MEMORY_TRACKING
	typedef int(*bfindreplaceImplSignature)(const char*, int, const char*, const char*, bstring* b0, const bstring* find, const bstring* replace, int pos);
#else
	typedef int(*bfindreplaceImplSignature)(bstring* b0, const bstring* find, const bstring* replace, int pos);
#endif

static int test31_0x1 (bfindreplaceImplSignature fn, const char* fnName, 
	bstring* b0, const bstring* find, const bstring* replace, int pos, const bstring* res, int resRv) {
	int rv, ret = 0;
	bstring copy = { 0, 0, NULL };
	if (bisvalid(b0))
		copy = bdynfromstr(b0, 0);
	else if (b0)
		copy = *b0;

#ifdef ENABLE_MEMORY_TRACKING
	rv = fn(__FILE__, __LINE__, __FUNCTION__, "", b0, find, replace, pos);
#else
	rv = fn(b0, find, replace, pos);
#endif
	ret += rv != resRv;

	if (rv == BSTR_OK)
		ret += !correctBstring(b0);

	if (bconstisvalid(res))
		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	else
		ret += bisvalid(b0);

	if (ret)
	{
		LOGF(eERROR, "  Expected: %s(%s, %s, %s, %d) = ", fnName,
					dumpBstring(&copy), dumpBstring(find), dumpBstring(replace), pos);
		LOGF(eERROR, "            % d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b0));
	}

	if (bisvalid(&copy))
		bdestroy(&copy);

	return ret;
}

static int test31_0(bstring* b0, const bstring* find, const bstring* replace, int pos, const bstring* res, int resRv)
{
	return test31_0x1(bfindreplaceImpl, "bfindreplace", b0, find, replace, pos, res, resRv);
}

static int test31_1(bstring* b0, const bstring* find, const bstring* replace, int pos, const bstring* res, int resRv)
{
	return test31_0x1(bfindreplacecaselessImpl, "bfindreplacecaseless", b0, find, replace, pos, res, resRv);
}

#define LOTS_OF_S "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss"

static int test31 (void) {

	int ret = 0;
	const bstring t0 = bconstfromliteral ("funny");
	const bstring t1 = bconstfromliteral("weird");
	const bstring t2 = bconstfromliteral("s");
	const bstring t3 = bconstfromliteral("long");
	const bstring t4 = bconstfromliteral("big");
	const bstring t5 = bconstfromliteral("ss");
	const bstring t6 = bconstfromliteral("sstsst");
	const bstring t7 = bconstfromliteral("xx" LOTS_OF_S "xx");
	const bstring t8 = bconstfromliteral("S");
	const bstring t9 = bconstfromliteral("LONG");

	LOGF(eINFO, "TEST 31: int bfindreplace (bstring* b, const bstring* f, const bstring* r, int pos);");
	/* tests with NULL */
	ret += test31_0 (NULL, NULL, NULL, 0, NULL, BSTR_ASSERT_ERR);
	ret += test31_0(NULL, &t1, &t1, 0, NULL, BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test31_0(str, NULL, &t1, 0, str, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* find = &badConstBstrings[j];
			ret += test31_0(str, find, &t1, 0, str, BSTR_ASSERT_ERR);
			ret += test31_0(str, &t1, find, 0, str, BSTR_ASSERT_ERR);

		}
	}

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test31_0(str, &t2, &t1, 0, NULL, BSTR_ASSERT_ERR);
	}

	/* normal operation tests */
	bstring tmp = bempty();
	bstring res; 
	const bstring* pRes = &res;

	res = bconstfromliteral("This is a funny but reasonably long string.  Just long enough to cause some mallocing.");
	ret += test31_0 (bassignaux(&tmp, LONG_STRING) , &shortBstring, &t0, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("Thiweird iweird a boguweird but reaweirdonably long weirdtring.  Juweirdt long enough to cauweirde weirdome mallocing.");
	ret += test31_0 (bassignaux(&tmp, LONG_STRING), &t2, &t1, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("boguweird");
	ret += test31_0 (bassignaux(&tmp, SHORT_STRING), &t2, &t1, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("bogus");
	ret += test31_0 (bassignaux(&tmp, SHORT_STRING), &t8, &t1, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("This is a bogus but reasonably long weirdtring.  Juweirdt long enough to cauweirde weirdome mallocing.");
	ret += test31_0 (bassignaux(&tmp, LONG_STRING), &t2, &t1, 27, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("This is a bogus but reasonably big string.  Just big enough to cause some mallocing.");
	ret += test31_0 (bassignaux(&tmp, LONG_STRING), &t3, &t4, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("This is a bogus but reasonably long string.  Just long enough to cause some mallocing.");
	ret += test31_0 (bassignaux(&tmp, LONG_STRING), &t9, &t4, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("sssstsssst");
	ret += test31_0 (bassignblkaux(&tmp, t6.data, t6.slen), &t2, &t5, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("xx" LOTS_OF_S LOTS_OF_S "xx");
	ret += test31_0 (bassignblkaux(&tmp, t7.data, t7.slen), &t2, &t5, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	/* Aliasing */
	res = bconstfromliteral(LONG_STRING);
	bassignliteral(&tmp, LONG_STRING);
	ret += test31_0(&tmp, &tmp, pRes, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	res = bconstfromliteral(LONG_STRING);
	bassignliteral(&tmp, LONG_STRING);
	ret += test31_0(&tmp, pRes, &tmp, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	res = bconstfromliteral(LONG_STRING);
	bassignliteral(&tmp, LONG_STRING);
	ret += test31_0(&tmp, &tmp, &tmp, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	LOGF(eINFO, "TEST 31: int bfindreplacecaseless (bstring* b, const bstring* f, const bstring* r, int pos);");
	///* tests with NULL */
	ret += test31_1(NULL, NULL, NULL, 0, NULL, BSTR_ASSERT_ERR);
	ret += test31_1(NULL, &t1, &t1, 0, NULL, BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* str = (bstring*)&pseudoGoodBstrings[i];
		ret += test31_1(str, NULL, &t1, 0, str, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			const bstring* find = &badConstBstrings[j];
			ret += test31_1(str, find, &t1, 0, str, BSTR_ASSERT_ERR);
			ret += test31_1(str, &t1, find, 0, str, BSTR_ASSERT_ERR);

		}
	}

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test31_1(str, &t2, &t1, 0, NULL, BSTR_ASSERT_ERR);
	}

	/* normal operation tests */
	res = bconstfromliteral("This is a funny but reasonably long string.  Just long enough to cause some mallocing.");
	ret += test31_1 (bassignaux(&tmp, LONG_STRING), &shortBstring, &t0, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("Thiweird iweird a boguweird but reaweirdonably long weirdtring.  Juweirdt long enough to cauweirde weirdome mallocing.");
	ret += test31_1 (bassignaux(&tmp, LONG_STRING), &t2, &t1, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("boguweird");
	ret += test31_1 (bassignaux(&tmp, SHORT_STRING), &t2, &t1, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("boguweird");
	ret += test31_1 (bassignaux(&tmp, SHORT_STRING), &t8, &t1, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("This is a bogus but reasonably long weirdtring.  Juweirdt long enough to cauweirde weirdome mallocing.");
	ret += test31_1 (bassignaux(&tmp, LONG_STRING), &t2, &t1, 27, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("This is a bogus but reasonably big string.  Just big enough to cause some mallocing.");
	ret += test31_1 (bassignaux(&tmp, LONG_STRING), &t3, &t4, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("This is a bogus but reasonably big string.  Just big enough to cause some mallocing.");
	ret += test31_1 (bassignaux(&tmp, LONG_STRING), &t9, &t4, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("sssstsssst");
	ret += test31_1 (bassignblkaux(&tmp, t6.data, t6.slen), &t2, &t5, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("sssstsssst");
	ret += test31_1 (bassignblkaux(&tmp, t6.data, t6.slen), &t8, &t5, 0, pRes, BSTR_OK);
	bdestroy(&tmp);
	res = bconstfromliteral("xx" LOTS_OF_S LOTS_OF_S "xx");
	ret += test31_1 (bassignblkaux(&tmp, t7.data, t7.slen), &t2, &t5, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	/* Aliasing */
	res = bconstfromliteral(LONG_STRING);
	bassignliteral(&tmp, LONG_STRING);
	ret += test31_1(&tmp, &tmp, pRes, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	res = bconstfromliteral(LONG_STRING);
	bassignliteral(&tmp, LONG_STRING);
	ret += test31_1(&tmp, pRes, &tmp, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	res = bconstfromliteral(LONG_STRING);
	bassignliteral(&tmp, LONG_STRING);
	ret += test31_1(&tmp, &tmp, &tmp, 0, pRes, BSTR_OK);
	bdestroy(&tmp);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test32_0 (const bstring* b, const char * s, int res) {
	int rv, ret = 0;

	ret += (res != (rv = biseqcstr (b, s)));
	if (ret) 
	{
		LOGF(eERROR, "  Expected: biseqcstr(%s, %s) = ", 
			dumpBstring(b), printablecstr(s));
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);	
	}
	return ret;
}

static int test32_1 (const bstring* b, const char * s, int res) {
	int rv, ret = 0;

	ret += (res != (rv = biseqcstrcaseless(b, s)));
	if (ret)
	{
		LOGF(eERROR, "  Expected: biseqcstrcaseless(%s, %s) = ", 
			dumpBstring(b), printablecstr(s));
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}


static int test32 (void) 
{
	int ret = 0;

	LOGF(eINFO, "TEST 32: int biseqcstr (const bstring* b, const char * s);");

	/* tests with NULL */
	ret += test32_0 (NULL, NULL, BSTR_ASSERT_ERR);
	ret += test32_0 (&emptyBstring, NULL, BSTR_ASSERT_ERR);
	ret += test32_0 (NULL, "", BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test32_0(str, "", BSTR_ASSERT_ERR);
	}


	/* normal operation tests on all sorts of subranges */
	ret += test32_0 (&emptyBstring, "", 1);
	ret += test32_0 (&shortBstring, "bogus", 1);
	ret += test32_0 (&emptyBstring, "bogus", 0);
	ret += test32_0 (&shortBstring, "", 0);

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b = &goodConstBstrings[i];
		for (int j = 0; j < (int)ARR_SIZE(cStrings); ++j)
		{
			const char* cstr = cStrings[j];
			int res = 0;
			if (strlen(cstr) == (size_t)b->slen &&
				strcmp(cstr, (const char*)b->data) == 0)
			{
				res = 1;
			}
			ret += test32_0(b, cstr, res);
		}
	}
	LOGF(eINFO, "\t# failures: %d", ret);

	LOGF(eINFO, "TEST 32: int biseqcstrcaseless (const bstring* b, const char * s);");

	/* tests with NULL */
	ret += test32_1(NULL, NULL, BSTR_ASSERT_ERR);
	ret += test32_1(&emptyBstring, NULL, BSTR_ASSERT_ERR);
	ret += test32_1(NULL, "", BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test32_1(str, "", BSTR_ASSERT_ERR);
	}

	///* normal operation tests on all sorts of subranges */
	ret += test32_1 (&emptyBstring, "", 1);
	ret += test32_1 (&shortBstring, "bogus", 1);
	ret += test32_1 (&shortBstring, "BOGUS", 1);
	ret += test32_1 (&emptyBstring, "bogus", 0);
	ret += test32_1 (&shortBstring, "", 0);

	for (int i = 0; i < (int)ARR_SIZE(goodConstBstrings); ++i)
	{
		const bstring* b = &goodConstBstrings[i];
		for (int j = 0; j < (int)ARR_SIZE(cStrings); ++j)
		{
			const char* cstr = cStrings[j];
			int res = 0;
			if (strlen(cstr) == (size_t)b->slen &&
				strcmp(cstr, (const char*)b->data) == 0)
			{
				res = 1;
			}
			ret += test32_1(b, cstr, res);
		}
	}

	LOGF(eINFO, "\t# failures: %d", ret);

	return ret;
}

static int test33_0 (bstring* b0, const bstring* res, int resRv) 
{
	int rv, ret = 0;
	bstring copy = { 0,0,NULL };
	if (bisvalid(b0))
		copy = bdynfromstr(b0, 0);
	else if (b0)
		copy = *b0;

	rv = btoupper(b0);
	ret += rv != resRv;
	
	if (bconstisvalid(res))
		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	else
		ret += bisvalid(b0);

	if (bisvalid(&copy))
		bdestroy(&copy);

	if (ret)
	{
		LOGF(eERROR, "  Expected: btoupper(%s) = ",
			dumpBstring(&copy));
		LOGF(eERROR, "            % d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b0));
	}

	return ret;
}

static int test33 (void) 
{
	int ret = 0;

	LOGF(eINFO, "TEST 33: int btoupper (bstring* b);");

	/* tests with NULL */
	ret += test33_0 (NULL, NULL, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test33_0(str, NULL, BSTR_ASSERT_ERR);
	}

	/* normal operation tests on all sorts of subranges */

	bstring b = bempty();
	bstring res = bempty();

	ret += test33_0 (&b, &res, BSTR_OK);
	bassignliteral(&b, SHORT_STRING);
	bassignliteral(&res, "BOGUS");
	ret += test33_0(&b, &res, BSTR_OK);
	bassignliteral(&b, LONG_STRING);
	bassignliteral(&res, "THIS IS A BOGUS BUT REASONABLY LONG STRING.  JUST LONG ENOUGH TO CAUSE SOME MALLOCING.");
	ret += test33_0(&b, &res, BSTR_OK);
	bassignliteral(&b, LONG_STRING_2);
	bassignliteral(&res, "THIS IS A BOGUS BUT REASONABLY LONG STRING.\0  JUST LONG ENOUGH TO CAUSE SOME MALLOCING.");
	ret += test33_0(&b, &res, BSTR_OK);

	bdestroy(&b);
	bdestroy(&res);

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	/* Just checks that bstring output is a "correct" bstring */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		ret += test33_0(str, str, BSTR_OK);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test34_0 (bstring* b0, const bstring* res, int resRv) {
	int rv, ret = 0;
	bstring copy = { 0,0,NULL };
	if (bisvalid(b0))
		copy = bdynfromstr(b0, 0);
	else if (b0)
		copy = *b0;

	rv = btolower(b0);
	ret += rv != resRv;

	if (bconstisvalid(res))
		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	else
		ret += bisvalid(b0);

	if (bisvalid(&copy))
		bdestroy(&copy);

	if (ret)
	{
		LOGF(eERROR, "  Expected: btolower(%s) = ",
			dumpBstring(&copy));
		LOGF(eERROR, "            % d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b0));
	}

	return ret;
}

static int test34 (void) 
{
	int ret = 0;

	LOGF(eINFO, "TEST 34: int btolower (bstring* b);");

	/* tests with NULL */
	ret += test34_0(NULL, NULL, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* str = (bstring*)&badBstrings[i];
		ret += test34_0(str, NULL, BSTR_ASSERT_ERR);
	}

	/* normal operation tests on all sorts of subranges */

	bstring b = bempty();
	bstring res = bempty();

	ret += test34_0(&b, &res, BSTR_OK);
	bassignliteral(&b, SHORT_STRING);
	bassignliteral(&res, "bogus");
	ret += test34_0(&b, &res, BSTR_OK);
	bassignliteral(&b, LONG_STRING);
	bassignliteral(&res, "this is a bogus but reasonably long string.  just long enough to cause some mallocing.");
	ret += test34_0(&b, &res, BSTR_OK);
	bassignliteral(&b, LONG_STRING_2);
	bassignliteral(&res, "this is a bogus but reasonably long string.\0  just long enough to cause some mallocing.");
	ret += test34_0(&b, &res, BSTR_OK);

	bdestroy(&b);
	bdestroy(&res);

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	/* Just checks that bstring output is a "correct" bstring */
	initGoodBstrings(&staticBufs, &strings);
	for (int i = 0; i < (int)ARR_SIZE(strings); ++i)
	{
		bstring* str = &strings[i];
		ret += test34_0(str, str, BSTR_OK);
	}
	deinitGoodBstrings(&staticBufs, &strings);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test35_0 (const bstring* b0, const bstring* b1, int res) {
	int rv, ret = 0;

	ret += (res != (rv = bstricmp (b0, b1)));
	if (ret) {
		LOGF(eERROR, "  Expected: bstricmp(%s, %s) = ",
			dumpBstring(b0), dumpBstring(b1));
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test35 (void) {
	int ret = 0;
	bstring t0 = bconstfromliteral("bOgUs");
	const bstring t1 = bconstfromliteral("bOgUR");
	const bstring t2 = bconstfromliteral("bOgUt");
	const bstring t3 = bconstfromliteral(SHORT_STRING "a");

	LOGF(eINFO, "TEST 35: int bstricmp (const bstring* b0, const bstring* b1);");

	/* tests with NULL */
	ret += test35_0 (NULL, NULL, BSTR_ASSERT_ERR);
	ret += test35_0 (&emptyBstring, NULL, BSTR_ASSERT_ERR);
	ret += test35_0 (NULL, &emptyBstring, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test35_0(&emptyBstring, str, BSTR_ASSERT_ERR);
		ret += test35_0(str, &emptyBstring, BSTR_ASSERT_ERR);
	}


	/* normal operation tests on all sorts of subranges */
	ret += test35_0 (&emptyBstring, &emptyBstring, 0);
	ret += test35_0 (&shortBstring, &t0, 0);
	ret += test35_0 (&shortBstring, &t1, tolower (shortBstring.data[4]) - tolower (t1.data[4]));
	ret += test35_0 (&shortBstring, &t2, tolower (shortBstring.data[4]) - tolower (t2.data[4]));
	ret += test35_0 (&shortBstring, &t3, -'a');
	ret += test35_0 (&t3, &shortBstring, 'a');


	t0.slen++;
	ret += test35_0 (&shortBstring, &t0, -BSTR_CMP_EXTRA_NULL);
	ret += test35_0 (&t0, &shortBstring,  BSTR_CMP_EXTRA_NULL);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test36_0 (const bstring* b0, const bstring* b1, int n, int res) 
{
	int rv, ret = 0;

	ret += (res != (rv = bstrnicmp (b0, b1, n)));
	if (ret) {
		LOGF(eERROR, "  Expected: bstrnicmp(%s, %s, %d) = ",
			dumpBstring(b0), dumpBstring(b1), n);
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test36 (void) 
{
	int ret = 0;
	bstring t0 = bconstfromliteral("bOgUs");
	bstring t1 = bconstfromliteral("bOgUR");
	bstring t2 = bconstfromliteral("bOgUt");
	bstring t3 = bconstfromliteral(SHORT_STRING "a");

	LOGF(eINFO, "TEST 36: int bstrnicmp (const bstring* b0, const bstring* b1);");

	/* tests with NULL */
	ret += test36_0 (NULL, NULL, 0, BSTR_ASSERT_ERR);
	ret += test36_0 (&emptyBstring, NULL, 0, BSTR_ASSERT_ERR);
	ret += test36_0 (NULL, &emptyBstring, 0, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test36_0(str, &emptyBstring, 0, BSTR_ASSERT_ERR);
		ret += test36_0(&emptyBstring, str, 0, BSTR_ASSERT_ERR);
		ret += test36_0(str, &emptyBstring, 5, BSTR_ASSERT_ERR);
		ret += test36_0(&emptyBstring, str, 5, BSTR_ASSERT_ERR);
	}

	/* normal operation tests on all sorts of subranges */
	ret += test36_0 (&emptyBstring, &emptyBstring, 0, 0);
	ret += test36_0 (&shortBstring, &t0, 0, 0);
	ret += test36_0 (&shortBstring, &t0, 5, 0);
	ret += test36_0 (&shortBstring, &t0, 4, 0);
	ret += test36_0 (&shortBstring, &t0, 6, 0);
	ret += test36_0 (&shortBstring, &t1, 5, shortBstring.data[4] - t1.data[4]);
	ret += test36_0 (&shortBstring, &t1, 4, 0);
	ret += test36_0 (&shortBstring, &t1, 6, shortBstring.data[4] - t1.data[4]);
	ret += test36_0 (&shortBstring, &t2, 5, shortBstring.data[4] - t2.data[4]);
	ret += test36_0 (&shortBstring, &t2, 4, 0);
	ret += test36_0 (&shortBstring, &t2, 6, shortBstring.data[4] - t2.data[4]);
	ret += test36_0(&shortBstring, &t3, INT_MAX, -'a');
	ret += test36_0(&t3, &shortBstring, INT_MAX,  'a');


	t0.slen++;
	ret += test36_0 (&shortBstring, &t0, 5, 0);
	ret += test36_0 (&shortBstring, &t0, 6, -BSTR_CMP_EXTRA_NULL);
	ret += test36_0 (&t0, &shortBstring, 6,  BSTR_CMP_EXTRA_NULL);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test37_0 (const bstring* b0, const bstring* b1, int res) 
{
	int rv, ret = 0;

	ret += (res != (rv = biseqcaseless (b0, b1)));
	if (ret) {
		LOGF(eERROR, "  Expected: biseqcaseless(%s, %s) = ",
			dumpBstring(b0), dumpBstring(b1));
		LOGF(eERROR, "            % d", res);
		LOGF(eERROR, "       Got: % d", rv);
	}
	return ret;
}

static int test37 (void) 
{
	int ret = 0;
	bstring t0 = bconstfromliteral("bOgUs");
	bstring t1 = bconstfromliteral("bOgUR");
	bstring t2 = bconstfromliteral("bOgUt");

	LOGF(eINFO, "TEST 37: int biseqcaseless (const bstring* b0, const bstring* b1);");

	/* tests with NULL */
	ret += test37_0 (NULL, NULL, BSTR_ASSERT_ERR);
	ret += test37_0 (&emptyBstring, NULL, BSTR_ASSERT_ERR);
	ret += test37_0 (NULL, &emptyBstring, BSTR_ASSERT_ERR);
	for (int i = 0; i < (int)ARR_SIZE(badConstBstrings); ++i)
	{
		const bstring* str = &badConstBstrings[i];
		ret += test37_0(&emptyBstring, str, BSTR_ASSERT_ERR);
		ret += test37_0(str, &emptyBstring, BSTR_ASSERT_ERR);
	}

	/* normal operation tests on all sorts of subranges */
	ret += test37_0 (&emptyBstring, &emptyBstring, 1);
	ret += test37_0 (&shortBstring, &t0, 1);
	ret += test37_0 (&shortBstring, &t1, 0);
	ret += test37_0 (&shortBstring, &t2, 0);
	ret += test37_0 (&shortBstring, &longBstring, 0);
	ret += test37_0 (&longBstring, &longBstring2, 0);


	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}


//static int test38_aux_bNgetc (struct emuFile * f) {
//int v = EOF;
//	if (NULL != f && EOF != (v = bchare (f->contents, f->ofs, EOF))) f->ofs++;
//	return v;
//}
//
//static size_t test38_aux_bNread (void *buff, size_t elsize, size_t nelem, struct emuFile * f) {
//char * b = (char *) buff;
//int v;
//size_t i, j, c = 0;
//
//	if (NULL == f || NULL == b) return c;
//	for (i=0; i < nelem; i++) for (j=0; j < elsize; j++) {
//		v = test38_aux_bNgetc (f);
//		if (EOF == v) {
//			*b = (char) '\0';
//			return c;
//		} else {
//			*b = (char) v;
//			b++;
//			c++;
//		}
//	}
//
//	return c;
//}
//
//static int test38_aux_bNopen (struct emuFile * f, bstring* b) {
//	if (NULL == f || NULL == b) return -__LINE__;
//	f->ofs = 0;
//	f->contents = b;
//	return 0;
//}
//
//static int test38 (void) {
//struct emuFile f;
//bstring* b0, b1, b2, b3;
//int ret = 0;
//
//	LOGF(eINFO, "TEST: bgets/breads test\n");
//
//	test38_aux_bNopen (&f, &shortBstring);
//
//	/* Creation/reads */
//
//	b0 = bgets ((bNgetc) test38_aux_bNgetc, &f, (char) 'b');
//	b1 = bread ((bNread) test38_aux_bNread, &f);
//	b2 = bgets ((bNgetc) test38_aux_bNgetc, &f, (char) '\0');
//	b3 = bread ((bNread) test38_aux_bNread, &f);
//
//	ret += 1 != biseqcstr (b0, "b");
//	ret += 1 != biseqcstr (b1, "ogus");
//	ret += NULL != b2;
//	ret += 1 != biseqcstr (b3, "");
//
//	/* Bogus accumulations */
//
//	f.ofs = 0;
//
//	ret += 0 <= bgetsa (NULL, (bNgetc) test38_aux_bNgetc, &f, (char) 'o');
//	ret += 0 <= breada (NULL, (bNread) test38_aux_bNread, &f);
//	ret += 0 <= bgetsa (&shortBstring, (bNgetc) test38_aux_bNgetc, &f, (char) 'o');
//	ret += 0 <= breada (&shortBstring, (bNread) test38_aux_bNread, &f);
//
//	/* Normal accumulations */
//
//	ret += 0 > bgetsa (b0, (bNgetc) test38_aux_bNgetc, &f, (char) 'o');
//	ret += 0 > breada (b1, (bNread) test38_aux_bNread, &f);
//
//	ret += 1 != biseqcstr (b0, "bbo");
//	ret += 1 != biseqcstr (b1, "ogusgus");
//
//	/* Attempt to append past end should do nothing */
//
//	ret += 0 > bgetsa (b0, (bNgetc) test38_aux_bNgetc, &f, (char) 'o');
//	ret += 0 > breada (b1, (bNread) test38_aux_bNread, &f);
//
//	ret += 1 != biseqcstr (b0, "bbo");
//	ret += 1 != biseqcstr (b1, "ogusgus");
//
//	bdestroy (b0);
//	bdestroy (b1);
//	bdestroy (b2);
//	bdestroy (b3);
//
//	if (ret) LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}
//

typedef int(*trimFn)(bstring* b);

static int test39_aux(trimFn fn, const char* fnName, bstring *b, const bstring* res, int resRv)
{
	int rv, ret = 0;
	bool slenEqMlen = b ? bmlen(b) == b->slen : false;
	bstring copy = { 0,0,NULL };
	if (bisvalid(b))
		copy = bdynfromstr(b, 0);
	else if (b)
		copy = *b;

	ret += resRv != (rv = fn(b));

	if (rv == BSTR_OK)
		ret += !correctBstring(b) && !slenEqMlen;

	if (bconstisvalid(res))
		ret += !bisvalid(b) || biseq(b, res) != 1;
	else
		ret += bisvalid(b);

	if (ret)
	{
		LOGF(eERROR, "  Expected: %s(%s) = ",
			 fnName, dumpBstring(&copy));
		LOGF(eERROR, "            % d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b));
	}
	
	if (bisvalid(&copy))
		bdestroy(&copy);
	return ret;
}

static int test39_0()
{
	static const trimFn fns[] =
	{
		brtrimws,
		bltrimws,
		btrimws
	};
	static const char* fnNames[] =
	{
		"brtrimws",
		"bltrimws",
		"btrimws"
	};
	int ret = 0;
	for (int i= 0; i < (int)ARR_SIZE(fns); ++i)
	{
		ret += test39_aux(fns[i], fnNames[i], NULL, NULL, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badBstrings); ++j)
		{
			bstring* str = (bstring*)&badBstrings[i];
			ret += test39_aux(fns[i], fnNames[i], str, NULL, BSTR_ASSERT_ERR);
		}
	}

	return ret;
}


static int test39_1(const bstring* b, const bstring* lRes, const bstring* rRes, const bstring* res)
{
	static const trimFn fns[] = 
	{
		bltrimws,
		brtrimws,
		btrimws
	};
	static const char* fnNames[] = 
	{
		"bltrimws",
		"brtrimws",
		"btrimws"
	};
	const bstring* results[] =
	{
		lRes,
		rRes,
		res
	};
	int ret = 0;

	unsigned char buf[STATIC_BUF_SIZE];
	unsigned char patternBuf[STATIC_BUF_SIZE];

	unsigned char* pBuf = &buf[MEM_OFS];
	bstring str = bempty();

	memset(&patternBuf[0], MEM_PATTERN, sizeof(patternBuf));

	ASSERT(
		bconstisvalid(b) &&
		bconstisvalid(lRes) &&
		bconstisvalid(rRes) &&
		bconstisvalid(res)
	);
	/*
	 * + statically/dynamically allocated
	 * + null terminated or not
	 * + with spare capacity and not
	 * + strings with null terminator in the middle
	 */
	for (int i = 0; i < (int)ARR_SIZE(results); ++i)
	{
		const trimFn fn = fns[i];
		const char* fnName = fnNames[i];
		res = results[i];
		int end = MEM_OFS + b->slen;
		if (b->slen == 0)
			end += 1;
		int realMlen = STATIC_BUF_SIZE - end;


		/* mlen == slen */

		ASSERT(b->slen < STATIC_BUF_SIZE - MEM_OFS - 100);
		memset(&buf[0], MEM_PATTERN, sizeof(buf));
		
		str.data = pBuf;
		str.slen = b->slen;
		str.mlen = end - MEM_OFS;

		memcpy(pBuf, b->data, b->slen);

		ret += test39_aux(fn, fnName, &str, res, BSTR_OK);

		ASSERT(memcmp(&buf[0], &patternBuf[0], MEM_OFS) == 0);
		ASSERT(memcmp(&buf[end], &patternBuf[0], realMlen) == 0);

		bdestroy(&str);


		/* mlen == slen + 1 */

		ASSERT(b->slen < STATIC_BUF_SIZE - MEM_OFS - 100);
		memset(&buf[0], MEM_PATTERN, sizeof(buf));

		end = MEM_OFS + b->slen + 1;
		realMlen = STATIC_BUF_SIZE - end;

		str.data = pBuf;
		str.slen = b->slen;
		str.mlen = end - MEM_OFS;

		memcpy(pBuf, b->data, b->slen);

		ret += test39_aux(fn, fnName, &str, res, BSTR_OK);

		ASSERT(memcmp(&buf[0], &patternBuf[0], MEM_OFS) == 0);
		ASSERT(memcmp(&buf[end], &patternBuf[0], realMlen) == 0);

		bdestroy(&str);


		/* mlen == slen + 1 null terminated */

		ASSERT(b->slen < STATIC_BUF_SIZE - MEM_OFS - 100);
		memset(&buf[0], MEM_PATTERN, sizeof(buf));

		end = MEM_OFS + b->slen + 1;
		realMlen = STATIC_BUF_SIZE - end;

		str.data = pBuf;
		str.slen = b->slen;
		str.mlen = end - MEM_OFS;

		memcpy(pBuf, b->data, b->slen);
		str.data[str.slen] = '\0';

		ret += test39_aux(fn, fnName, &str, res, BSTR_OK);

		ASSERT(memcmp(&buf[0], &patternBuf[0], MEM_OFS) == 0);
		ASSERT(memcmp(&buf[end], &patternBuf[0], realMlen) == 0);

		bdestroy(&str);
	}

	return ret;
	
}

static int test39 (void) {
	int ret = 0;
	const bstring t0 = bconstfromliteral("   bogus string   ");
	const bstring t1 = bconstfromliteral("bogus string   ");
	const bstring t2 = bconstfromliteral("   bogus string");
	const bstring t3 = bconstfromliteral("bogus string");
	const bstring t4 = bconstfromliteral("     ");
	const bstring t5 = bconstfromliteral("");

	LOGF(eINFO, "TEST 39: trim functions");

	ret += test39_0();

	ret += test39_1 (&t0, &t1, &t2, &t3);
	ret += test39_1 (&t1, &t1, &t3, &t3);
	ret += test39_1 (&t2, &t3, &t2, &t3);
	ret += test39_1 (&t3, &t3, &t3, &t3);
	ret += test39_1 (&t4, &t5, &t5, &t5);
	ret += test39_1 (&t5, &t5, &t5, &t5);

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

static int test40_0 (bstring* b0, const bstring* b1, int left, int len, const bstring* res, int resRv) 
{
	int rv, ret = 0;
	bstring copy = { 0,0,NULL };
	if (bisvalid(b0))
		copy = bdynfromstr(b0, 0);
	else if (b0)
		copy = *b0;

	rv = bassignmidstr(b0, b1, left, len);
	ret += rv != resRv;

	if (rv == BSTR_OK)
		ret += !correctBstring(b0);

	if (bconstisvalid(res))
		ret += !bisvalid(b0) || biseq(b0, res) != 1;
	else
		ret += bisvalid(b0);

	if (ret)
	{
		LOGF(eERROR, "  Expected: bassignmidstr(%s, %s, %d, %d) = ",
			dumpBstring(&copy), dumpBstring(b1), left, len);
		LOGF(eERROR, "            % d, %s", resRv, dumpBstring(res));
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b0));
	}

	if (bisvalid(&copy))
		bdestroy(&copy);

	return ret;
}

static int test40 (void) 
{
	int ret = 0;

	LOGF(eINFO, "TEST 40: int bassignmidstr (bstring* b0, const bstring* b1, int left, int len);");

	/* tests with NULL */
	ret += test40_0 (NULL, NULL, 0, 1, NULL, BSTR_ASSERT_ERR);
	ret += test40_0 (NULL, &emptyBstring, 0, 1, NULL, BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* b0 = (bstring*)&badBstrings[i];
		ret += test40_0(b0, NULL, 0, 1, NULL, BSTR_ASSERT_ERR);
		ret += test40_0(NULL, b0, 0, 1, NULL, BSTR_ASSERT_ERR);
		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			ret += test40_0(b0, &badConstBstrings[j], 0, 1, NULL, BSTR_ASSERT_ERR);
		}
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b0 = (bstring*)&pseudoGoodBstrings[i];
		ret += test40_0(b0, NULL, 0, 1, b0, BSTR_ASSERT_ERR);
		ret += test40_0(NULL, b0, 0, 1, NULL, BSTR_ASSERT_ERR);
		ret += test40_0(b0, b0, -1,  1, b0, BSTR_ASSERT_ERR);
		ret += test40_0(b0, b0,  1, -1, b0, BSTR_ASSERT_ERR);
		ret += test40_0(b0, b0, -1, -1, b0, BSTR_ASSERT_ERR);
		ret += test40_0(b0, &emptyBstring, 9, 1, b0, BSTR_ASSERT_ERR);

		for (int j = 0; j < (int)ARR_SIZE(badConstBstrings); ++j)
		{
			ret += test40_0(b0, &badConstBstrings[j], 0, 1, b0, BSTR_ASSERT_ERR);

		}
	}

	/* normal operation tests on all sorts of subranges */
	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	const bstring results[] = {
		bconstfromliteral(""),
		bconstfromliteral("ogu"),
		bconstfromliteral(""),
		bconstfromliteral("ogu"),
		bconstfromliteral("ogus"),
	};

	static const bstring* inputs[] = {
		&emptyBstring,
		&shortBstring,
		&emptyBstring,
		&shortBstring,
		&shortBstring,
	};

	const int lefts[] = {
		0,
		1,
		0,
		1,
		1,
	};

	static const int lens[] = {
		1,
		3,
		1,
		3,
		9,
	};

	
	for (int i = 0; i < (int)ARR_SIZE(results); ++i)
	{
		const bstring* b1 = inputs[i];
		int left = lefts[i];
		int len = lens[i];
		const bstring* res = &results[i];

		initGoodBstrings(&staticBufs, &strings);

		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b0 = &strings[j];
			ret += test40_0(b0, b1, left, len, res, BSTR_OK);
		}

		deinitGoodBstrings(&staticBufs, &strings);
	}


	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

//static int test41_0 (bstring* b1, int left, int len, const char * res) {
//bstring t;
//bstring* b2, b3;
//int ret = 0;
//
//	if (b1 != NULL && b1->data != NULL && b1->slen >= 0) {
//		b2 = bfromcstr ("");
//
//		bassignmidstr (b2, b1, left, len);
//
//		bmid2tbstr (t, b1, left, len);
//		b3 = bstrcpy (&t);
//
//		LOGF(eINFO, ".\tbmid2tbstr (%s, %d, %d) = %s\n", dumpBstring (b1), left, len, dumpBstring (b3));
//
//		ret += !biseq (&t, b2);
//
//		bdestroy (b2);
//		bdestroy (b3);
//	} else {
//		bmid2tbstr (t, b1, left, len);
//		b3 = bstrcpy (&t);
//		ret += t.slen != 0;
//
//		LOGF(eINFO, ".\tbmid2tbstr (%s, %d, %d) = %s\n", dumpBstring (b1), left, len, dumpBstring (b3));
//		bdestroy (b3);
//	}
//
//	if (ret) {
//		LOGF(eINFO, "\t\tfailure(%d) = %d (res = %p", __LINE__, ret, res);
//		if (res) LOGF(eINFO, " = \"%s\"", res);
//		LOGF(eINFO, ")\n");
//	}
//	return ret;
//}
//
//static int test41 (void) {
//int ret = 0;
//
//	LOGF(eINFO, "TEST: int bmid2tbstr (bstring &t, const bstring* b1, int left, int len);\n");
//
//	/* tests with NULL */
//	ret += test41_0 (NULL, 0, 1, NULL);
//	ret += test41_0 (&emptyBstring, 0, 1, NULL);
//	ret += test41_0 (NULL, 0, 1, "");
//	ret += test41_0 (&emptyBstring, 0, 1, NULL);
//	ret += test41_0 (&emptyBstring, 0, 1, NULL);
//	ret += test41_0 (&badBstring1, 0, 1, NULL);
//	ret += test41_0 (&badBstring2, 0, 1, NULL);
//
//	/* normal operation tests on all sorts of subranges */
//	ret += test41_0 (&emptyBstring, 0, 1, "");
//	ret += test41_0 (&shortBstring, 1, 3, "ogu");
//	ret += test41_0 (&emptyBstring, 0, 1, "");
//	ret += test41_0 (&shortBstring, 1, 3, "ogu");
//	ret += test41_0 (&shortBstring, -1, 4, "bog");
//	ret += test41_0 (&shortBstring, 1, 9, "ogus");
//	ret += test41_0 (&shortBstring, 9, 1, "");
//
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}

static int test42_0 (bstring* b, int len, int resRv) {
	int rv, ret = 0;
	
	bstring res = { 0,0,NULL };
	bstring copy = { 0,0,NULL };
	if (bisvalid(b))
	{
		if (resRv == BSTR_OK)
			res = bdynfromblk(b->data, len, 0);
		else
			res = bdynfromstr(b, 0);

		copy = bdynfromstr(b, 0);
		ASSERT(bisvalid(&copy));
	}
	else if (b)
		copy = *b;
	
	rv = btrunc(b, len);
	ret += rv != resRv;

	if (bisvalid(&res))
		ret += !bisvalid(b) || biseq(b, &res) != 1;
	else
		ret += bisvalid(b);

	if (ret)
	{
		LOGF(eERROR, "  Expected: btrunc(%s, %d) = ",
			dumpBstring(&copy), len);
		LOGF(eERROR, "            % d, %s", resRv, dumpBstring(&res));
		LOGF(eERROR, "       Got: % d, %s", rv, dumpBstring(b));
	}

	if (bisvalid(&res))
	{
		bdestroy(&res);
		bdestroy(&copy);
	}

	return ret;
}

static int test42 (void) 
{
	int ret = 0;

	LOGF(eINFO, "TEST 42: int btrunc (bstring* b, int n);");

	/* tests with NULL */
	ret += test42_0(NULL, 2, BSTR_ASSERT_ERR);
	ret += test42_0(NULL, 0, BSTR_ASSERT_ERR);
	ret += test42_0(NULL, -1, BSTR_ASSERT_ERR);

	for (int i = 0; i < (int)ARR_SIZE(badBstrings); ++i)
	{
		bstring* b = (bstring*)&badBstrings[i];
		ret += test42_0(b, 2, BSTR_ASSERT_ERR);
		ret += test42_0(b, 0, BSTR_ASSERT_ERR);
		ret += test42_0(b, -1, BSTR_ASSERT_ERR);
	}

	for (int i = 0; i < (int)ARR_SIZE(pseudoGoodBstrings); ++i)
	{
		bstring* b = (bstring*)&pseudoGoodBstrings[i];
		ret += test42_0(b, -1, BSTR_ASSERT_ERR);
	}

	unsigned char staticBufs[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	for (int i = 0; i < 10; ++i)
	{
		initGoodBstrings(&staticBufs, &strings);
		for (int j = 0; j < (int)ARR_SIZE(strings); ++j)
		{
			bstring* b = &strings[j];
			if (i > b->slen)
				continue;

			ret += test42_0(b, i, BSTR_OK);
		}
		deinitGoodBstrings(&staticBufs, &strings);
	}

	LOGF(eINFO, "\t# failures: %d", ret);
	return ret;
}

//static int test43 (void) {
//	static bstring ts0 = bsStatic ("");
//	static bstring ts1 = bsStatic ("    ");
//	static bstring ts2 = bsStatic (" abc");
//	static bstring ts3 = bsStatic ("abc ");
//	static bstring ts4 = bsStatic (" abc ");
//	static bstring ts5 = bsStatic ("abc");
//	bstring* tstrs[6] = { &ts0, &ts1, &ts2, &ts3, &ts4, &ts5 };
//	int ret = 0;
//	int i;
//
//	LOGF(eINFO, "TEST 43: int btfromblk*trim (bstring t, void * s, int l);");
//
//	for (i=0; i < 6; i++) {
//		bstring t;
//		bstring* b;
//
//		btfromblkltrimws (t, tstrs[i]->data, tstrs[i]->slen);
//		bltrimws (b = bstrcpy (tstrs[i]));
//		if (!biseq (b, &t)) {
//			ret++;
//			bassign (b, &t);
//			LOGF(eINFO, "btfromblkltrimws failure: <%s> -> <%s>\n", tstrs[i]->data, b->data);
//		}
//		LOGF(eINFO, ".\tbtfromblkltrimws (\"%s\", \"%s\", %d)\n", (char *) bdatae (b, NULL), tstrs[i]->data, tstrs[i]->slen);
//		bdestroy (b);
//
//		btfromblkrtrimws (t, tstrs[i]->data, tstrs[i]->slen);
//		brtrimws (b = bstrcpy (tstrs[i]));
//		if (!biseq (b, &t)) {
//			ret++;
//			bassign (b, &t);
//			LOGF(eINFO, "btfromblkrtrimws failure: <%s> -> <%s>\n", tstrs[i]->data, b->data);
//		}
//		LOGF(eINFO, ".\tbtfromblkrtrimws (\"%s\", \"%s\", %d)\n", (char *) bdatae (b, NULL), tstrs[i]->data, tstrs[i]->slen);
//		bdestroy (b);
//
//		btfromblktrimws (t, tstrs[i]->data, tstrs[i]->slen);
//		btrimws (b = bstrcpy (tstrs[i]));
//		if (!biseq (b, &t)) {
//			ret++;
//			bassign (b, &t);
//			LOGF(eINFO, "btfromblktrimws failure: <%s> -> <%s>\n", tstrs[i]->data, b->data);
//		}
//		LOGF(eINFO, ".\tbtfromblktrimws (\"%s\", \"%s\", %d)\n", (char *) bdatae (b, NULL), tstrs[i]->data, tstrs[i]->slen);
//		bdestroy (b);
//	}
//
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}

//static int test44_0 (const char * str) {
//int ret = 0, v;
//bstring* b;
//	if (NULL == str) {
//		ret += 0 <= bassigncstr (NULL, "test");
//		LOGF(eINFO, ".\tbassigncstr (b = %s, NULL)", dumpBstring (b = bfromcstr ("")));
//		ret += 0 <= (v = bassigncstr (b, NULL));
//		LOGF(eINFO, " = %d; b -> %s\n", v, dumpBstring (b));
//		ret += 0 <= bassigncstr (&shortBstring, NULL);
//		bdestroy (b);
//		return ret;
//	}
//
//	ret += 0 <= bassigncstr (NULL, str);
//	LOGF(eINFO, ".\tbassigncstr (b = %s, \"%s\")", dumpBstring (b = bfromcstr ("")), str);
//	ret += 0 > (v = bassigncstr (b, str));
//	LOGF(eINFO, " = %d; b -> %s\n", v, dumpBstring (b));
//	ret += 0 != strcmp (bdatae (b, ""), str);
//	ret += ((size_t) b->slen) != strlen (str);
//	ret += 0 > bassigncstr (b, "xxxxx");
//	bwriteprotect(*b)
//	LOGF(eINFO, ".\tbassigncstr (b = %s, \"%s\")", dumpBstring (b), str);
//	ret += 0 <= (v = bassigncstr (b, str));
//	LOGF(eINFO, " = %d; b -> %s\n", v, dumpBstring (b));
//	ret += 0 != strcmp (bdatae (b, ""), "xxxxx");
//	ret += ((size_t) b->slen) != strlen ("xxxxx");
//	bwriteallow(*b)
//	ret += 0 <= bassigncstr (&shortBstring, str);
//	bdestroy (b);
//	LOGF(eINFO, ".\tbassigncstr (a = %s, \"%s\")", dumpBstring (&shortBstring), str);
//	ret += 0 <= (v = bassigncstr (&shortBstring, str));
//	LOGF(eINFO, " = %d; a -> %s\n", v, dumpBstring (&shortBstring));
//	return ret;
//}
//
//static int test44 (void) {
//int ret = 0;
//
//	LOGF(eINFO, "TEST: int bassigncstr (bstring* a, char * str);\n");
//
//	/* tests with NULL */
//	ret += test44_0 (NULL);
//
//	ret += test44_0 (EMPTY_STRING);
//	ret += test44_0 (SHORT_STRING);
//	ret += test44_0 (LONG_STRING);
//
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}
//
//static int test45_0 (const char * str) {
//int ret = 0, v, len;
//bstring* b;
//	if (NULL == str) {
//		ret += 0 <= bassignblk (NULL, "test", 4);
//		LOGF(eINFO, ".\tbassignblk (b = %s, NULL, 1)", dumpBstring (b = bfromcstr ("")));
//		ret += 0 <= (v = bassignblk (b, NULL, 1));
//		LOGF(eINFO, " = %d; b -> %s\n", v, dumpBstring (b));
//		ret += 0 <= bassignblk (&shortBstring, NULL, 1);
//		bdestroy (b);
//		return ret;
//	}
//
//	len = (int) strlen (str);
//	ret += 0 <= bassignblk (NULL, str, len);
//	LOGF(eINFO, ".\tbassignblk (b = %s, \"%s\", %d)", dumpBstring (b = bfromcstr ("")), str, len);
//	ret += 0 > (v = bassignblk (b, str, len));
//	LOGF(eINFO, " = %d; b -> %s\n", v, dumpBstring (b));
//	ret += 0 != strcmp (bdatae (b, ""), str);
//	ret += b->slen != len;
//	ret += 0 > bassigncstr (b, "xxxxx");
//	bwriteprotect(*b)
//	LOGF(eINFO, ".\tbassignblk (b = %s, \"%s\", %d)", dumpBstring (b), str, len);
//	ret += 0 <= (v = bassignblk (b, str, len));
//	LOGF(eINFO, " = %d; b -> %s\n", v, dumpBstring (b));
//	ret += 0 != strcmp (bdatae (b, ""), "xxxxx");
//	ret += ((size_t) b->slen) != strlen ("xxxxx");
//	bwriteallow(*b)
//	ret += 0 <= bassignblk (&shortBstring, str, len);
//	bdestroy (b);
//	LOGF(eINFO, ".\tbassignblk (a = %s, \"%s\", %d)", dumpBstring (&shortBstring), str, len);
//	ret += 0 <= (v = bassignblk (&shortBstring, str, len));
//	LOGF(eINFO, " = %d; a -> %s\n", v, dumpBstring (&shortBstring));
//	return ret;
//}
//
//static int test45 (void) {
//int ret = 0;
//
//	LOGF(eINFO, "TEST: int bassignblk (bstring* a, const void * s, int len);\n");
//
//	/* tests with NULL */
//	ret += test45_0 (NULL);
//
//	ret += test45_0 (EMPTY_STRING);
//	ret += test45_0 (SHORT_STRING);
//	ret += test45_0 (LONG_STRING);
//
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}
//
//static int test46_0 (const bstring* r, bstring* b, int count, const char * fmt, ...) {
//int ret;
//va_list arglist;
//
//	LOGF(eINFO, ".\tbvcformata (%s, %d, \"%s\", ...) -> ", dumpBstring (b), count, fmt);
//	va_start (arglist, fmt);
//	ret = bvcformata (b, count, fmt, arglist);
//	va_end (arglist);
//	LOGF(eINFO, "%d, %s (%s)\n", ret, dumpBstring (b), dumpBstring (r));
//	if (ret < 0) return (NULL != r);
//	ret += 1 != biseq (r, b);
//	if (0 != ret) LOGF(eINFO, "\t->failed\n");
//	return ret;
//}
//
//static int test46_1 (bstring* b, const char * fmt, const bstring* r, ...) {
//int ret;
//
//	LOGF(eINFO, ".\tbvformata (&, %s, \"%s\", ...) -> ", dumpBstring (b), fmt);
// 	bvformata (ret, b, fmt, r);
//	LOGF(eINFO, "%d, %s (%s)\n", ret, dumpBstring (b), dumpBstring (r));
//	if (ret < 0) return (NULL != r);
//	ret += 1 != biseq (r, b);
//	if (0 != ret) LOGF(eINFO, "\t->failed\n");
//	return ret;
//}
//
//static int test46 (void) {
//bstring* b, b2;
//int ret = 0;
//
//	LOGF(eINFO, "TEST: int bvcformata (bstring* b, int count, const char * fmt, va_list arg);\n");
//
//	ret += test46_0 (NULL, NULL, 8, "[%d]", 15);
//	ret += test46_0 (NULL, &shortBstring, 8, "[%d]", 15);
//	ret += test46_0 (NULL, &badBstring1, 8, "[%d]", 15);
//	ret += test46_0 (NULL, &badBstring2, 8, "[%d]", 15);
//	ret += test46_0 (NULL, &badBstring3, 8, "[%d]", 15);
//
//	b = bfromcstr ("");
//	ret += test46_0 (&shortBstring, b, shortBstring.slen, "%s", (char *) shortBstring.data);
//	b->slen = 0;
//	ret += test46_0 (&shortBstring, b, shortBstring.slen + 1, "%s", (char *) shortBstring.data);
//	b->slen = 0;
//	ret += test46_0 (NULL, b, shortBstring.slen-1, "%s", (char *) shortBstring.data);
//
//	LOGF(eINFO, "TEST: bvformata (int &ret, bstring* b, const char * fmt, <type> lastarg);\n");
//
//	ret += test46_1 (NULL, "[%d]", NULL, 15);
//	ret += test46_1 (&shortBstring, "[%d]", NULL, 15);
//	ret += test46_1 (&badBstring1, "[%d]", NULL, 15);
//	ret += test46_1 (&badBstring2, "[%d]", NULL, 15);
//	ret += test46_1 (&badBstring3, "[%d]", NULL, 15);
//
//	b->slen = 0;
//	ret += test46_1 (b, "%s", &shortBstring, (char *) shortBstring.data);
//
//	b->slen = 0;
//	ret += test46_1 (b, "%s", &longBstring, (char *) longBstring.data);
//
//	b->slen = 0;
//	b2 = bfromcstr (EIGHT_CHAR_STRING);
//	bconcat (b2, b2);
//	bconcat (b2, b2);
//	bconcat (b2, b2);
//	ret += test46_1 (b, "%s%s%s%s%s%s%s%s", b2,
//	                 EIGHT_CHAR_STRING, EIGHT_CHAR_STRING, EIGHT_CHAR_STRING, EIGHT_CHAR_STRING,
//	                 EIGHT_CHAR_STRING, EIGHT_CHAR_STRING, EIGHT_CHAR_STRING, EIGHT_CHAR_STRING);
//	bdestroy (b2);
//
//	bdestroy (b);
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}



static tfrg_atomic32_t gBstringTestDone = false;



#ifdef __has_feature
#if __has_feature(thread_sanitizer)
#define NO_TSAN __attribute__((no_sanitize("thread")))
#endif
#endif
#ifndef NO_TSAN
#define NO_TSAN
#endif

static void emptyStringCheck(void*)
{
//  TODO: Uncomment when atomics are fixed
//	while (tfrg_atomic32_load_acquire(&gBstringTestDone) == false)
//	{
//		ASSERT(gEmptyStringBuffer[0] == '\0');
//	}
//  TODO: Delete when atomics are fixed
    uint32_t done = false;
    while (!done)
    {
        tfrg_memorybarrier_acquire();
        done = gBstringTestDone;
        tfrg_memorybarrier_release();
        ASSERT(gEmptyStringBuffer[0] == '\0');
    }
}

NO_TSAN static void setBstrlibTestDone()
{
//  TODO: Uncomment when atomics are fixed
//  tfrg_atomic32_store_release(&gBstringTestDone, true);
//  TODO: Delete when atomics are fixed
    tfrg_memorybarrier_acquire();
    gBstringTestDone = true;
    tfrg_memorybarrier_release();
}

static int runBstringTests () {
	int success = 0;
	int i,ret = 0;

	static int(* const tests[])() =
	{
		test0,
		test1,
		test2,
		test3,
		test4,
		test5,
		test6,
		test7,
		test8,
		test47,
		test9,
		test10,
		test11,
		test12,
		test13,
		test14,
		test15,
		test16,
		test17,
		test18,
		test19,
		test20,
		test21,
		test22,

		test24,
		test25,
		test26,

		test28,
		test29,
		test31,
		test32,
		test33,
		test34,
		test35,
		test36,
		test37,
		test39,
		test40,
		test42,
	};

	unsigned char buffers[STATIC_BUF_COUNT][STATIC_BUF_SIZE];
	bstring strings[TOTAL_GOOD_STRING_COUNT];

	ThreadHandle emptyStringCheckHandle;
	ThreadDesc threadDesc = {};
	threadDesc.pFunc = emptyStringCheck;
	threadDesc.pData = NULL;
	strncpy(threadDesc.mThreadName, "EmptyBstringChecker", MAX_THREAD_NAME_LENGTH);
	initThread(&threadDesc, &emptyStringCheckHandle);
	


	/* Check that there are no errors in just init and deinit strings */
	initGoodBstrings(&buffers, &strings);
	deinitGoodBstrings(&buffers, &strings);

	LOGF(eINFO, "Direct case testing of bstring* core functions");


	for (i = 0; i < (int)ARR_SIZE(tests) && ret == 0; ++i)
	{
		int result = tests[i]();
		success += result == 0;
		ret += result;
	}

	LOGF(eINFO, "# test failures: %d", ret);
	LOGF(eINFO, "# test successes: %d", success);
	
	for (int j = 0; j < (int)ARR_SIZE(dumpOut); ++j)
		bdestroy(&dumpOut[j]);

    setBstrlibTestDone();
	joinThread(emptyStringCheckHandle);

	return ret;
}

// REMOVED TESTS

//static int test2_0 (const bstring* b, char z, const unsigned char * res) {
//	char * s = bstr2cstr (b, z);
//	int ret = 0;
//	if (s == NULL) {
//		if (res != NULL) ret++;
//		LOGF(eINFO, ".\tbstr2cstr (%s, %02X) = NULL\n", dumpBstring (b), z);
//		free(s);
//		return ret;
//	}
//
//	if (res == NULL) ret++;
//	else {
//		if (z != '\0') if ((int) strlen (s) != b->slen) ret++;
//		if (!ret) {
//			ret += (0 != memcmp (res, b->data, b->slen));
//		}
//	}
//
//	LOGF(eINFO, ".\tbstr2cstr (%s, %02X) = \"%s\"\n", dumpBstring (b), z, s);
//	free (s);
//	return ret;
//}
//
//bstring emptyBstring = bsStatic ("");
//bstring shortBstring = bsStatic ("bogus");
//bstring longBstring  = bsStatic ("This is a bogus but reasonably long string.  Just long enough to cause some mallocing.");
//
//bstring badBstring1 = {8,  4, NULL};
//bstring badBstring2 = {2, -5, (unsigned char *) "bogus"};
//bstring badBstring3 = {2,  5, (unsigned char *) "bogus"};
//
//bstring xxxxxBstring = bsStatic ("xxxxx");
//
//static int test2 (void) {
//int ret = 0;
//
//	LOGF(eINFO, "TEST: char * bstr2cstr (const bstring* s, char z);\n");
//
//	/* tests with NULL */
//	ret += test2_0 (NULL, (char) '?', NULL);
//
//	/* normal operation tests */
//	ret += test2_0 (&emptyBstring, (char) '?', emptyBstring.data);
//	ret += test2_0 (&shortBstring, (char) '?', shortBstring.data);
//	ret += test2_0 (&longBstring, (char) '?', longBstring.data);
//	ret += test2_0 (&badBstring1, (char) '?', NULL);
//	ret += test2_0 (&badBstring2, (char) '?', NULL);
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}
//
//static int test3_0 (const bstring* b) {
//bstring* b0 = bstrcpy (b);
//int ret = 0;
//	LOGF(eINFO, ".\tbstrcpy (%s) = %s\n", dumpBstring (b), dumpBstring (b0));
//	if (b0 == NULL) {
//		if (b != NULL && b->data != NULL && b->slen >= 0) ret++;
//	} else {
//		ret += (b == NULL) || (b->slen != b0->slen)
//		       || (0 != memcmp (b->data, b0->data, b->slen));
//		ret += b0->data[b0->slen] != '\0';
//	}
//	bdestroy (b0);
//	return ret;
//}
//
//static int test3 (void) {
//int ret = 0;
//
//	LOGF(eINFO, "TEST: bstring* bstrcpy (const bstring* b1);\n");
//
//	/* tests with NULL to make sure that there is NULL propogation */
//	ret += test3_0 (NULL);
//	ret += test3_0 (&badBstring1);
//	ret += test3_0 (&badBstring2);
//
//	/* normal operation tests */
//	ret += test3_0 (&emptyBstring);
//	ret += test3_0 (&shortBstring);
//	ret += test3_0 (&longBstring);
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}
//
//static int test4_0 (const bstring* b, int left, int len, const char * res) {
//bstring* b0 = bmidstr (b, left, len);
//int ret = 0;
//	LOGF(eINFO, ".\tbmidstr (%s, %d, %d) = %s\n", dumpBstring (b), left, len, dumpBstring (b0));
//	if (b0 == NULL) {
//		if (b != NULL && b->data != NULL && b->slen >= 0 && len >= 0) ret++;
//	} else {
//		ret += (b == NULL) || (res == NULL) || (b0->slen > len && len >= 0)
//		       || (b0->slen != (int) strlen (res))
//		       || (b0->slen > 0 && 0 != memcmp (res, b0->data, b0->slen));
//		ret += b0->data[b0->slen] != '\0';
//	}
//	if (ret) {
//		LOGF(eINFO, "(b == NULL)                  = %d\n", (b == NULL));
//		LOGF(eINFO, "(res == NULL)                = %d\n", (res == NULL));
//		LOGF(eINFO, "(b0->slen > len && len >= 0) = %d\n", (b0->slen > len && len >= 0));
//		if (res) LOGF(eINFO, "(b0->slen != strlen (res))   = %d\n", (b0->slen != (int) strlen (res)));
//		LOGF(eINFO, "(b0->slen > 0 && 0 != memcmp (res, b0->data, b0->slen) = %d\n", (b0->slen > 0 && 0 != memcmp (res, b0->data, b0->slen)));
//
//		LOGF(eINFO, "\t\tfailure(%d) = %d (res = %p", __LINE__, ret, res);
//		if (res) LOGF(eINFO, " = \"%s\"", res);
//		LOGF(eINFO, ")\n");
//	}
//	bdestroy (b0);
//	return ret;
//}
//
//static int test4 (void) {
//int ret = 0;
//
//	LOGF(eINFO, "TEST: bstring* bmidstr (const bstring* b, int left, int len);\n");
//
//	/* tests with NULL to make sure that there is NULL propogation */
//	ret += test4_0 (NULL,  0,  0, NULL);
//	ret += test4_0 (NULL,  0,  2, NULL);
//	ret += test4_0 (NULL,  0, -2, NULL);
//	ret += test4_0 (NULL, -5,  2, NULL);
//	ret += test4_0 (NULL, -5, -2, NULL);
//	ret += test4_0 (&badBstring1, 1, 3, NULL);
//	ret += test4_0 (&badBstring2, 1, 3, NULL);
//
//	/* normal operation tests on all sorts of subranges */
//	ret += test4_0 (&emptyBstring,  0,  0, "");
//	ret += test4_0 (&emptyBstring,  0, -1, "");
//	ret += test4_0 (&emptyBstring,  1,  3, "");
//	ret += test4_0 (&shortBstring,  0,  0, "");
//	ret += test4_0 (&shortBstring,  0, -1, "");
//	ret += test4_0 (&shortBstring,  1,  3, "ogu");
//	ret += test4_0 (&shortBstring, -1,  3, "bo");
//	ret += test4_0 (&shortBstring, -1,  9, "bogus");
//	ret += test4_0 (&shortBstring,  3, -1, "");
//	ret += test4_0 (&shortBstring,  9,  3, "");
//	LOGF(eINFO, "\t# failures: %d\n", ret);
//	return ret;
//}

#ifdef __cplusplus
}
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif
