/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002-2015, and is covered by the BSD open source
 * license and the GPL. Refer to the accompanying documentation for details
 * on usage and license.
 */

/*
 * utf8util.c
 *
 * This file is not necessarily part of the core bstring library itself, but
 * is just an generic module for implementing utf8 utility functions.
 */

#include "utf8util.h"

#ifndef NULL
#ifdef __cplusplus
#define NULL	0
#else
#define NULL	((void *)0)
#endif
#endif

/* Surrogate range is wrong, there is a maximum, the BOM alias is illegal and 0xFFFF is illegal */
#define isLegalUnicodeCodePoint(v) ((((v) < 0xD800L) || ((v) > 0xDFFFL)) && (((unsigned long)(v)) <= 0x0010FFFFL) && (((v)|0x1F0001) != 0x1FFFFFL))

void utf8IteratorInit (struct utf8Iterator* iter, unsigned char* data, int slen) {
	if (iter) {
		iter->data  = data;
		iter->slen  = (iter->data && slen >= 0) ? slen : -1;
		iter->start = -1;
		iter->next  = (iter->slen >= 0) ? 0 : -1;
		iter->error = (iter->slen >= 0) ? 0 : 1;
	}
}

void utf8IteratorUninit (struct utf8Iterator* iter) {
	if (iter) {
		iter->data  = NULL;
		iter->slen  = -1;
		iter->start = iter->next = -1;
	}
}

int utf8ScanBackwardsForCodePoint (unsigned char* msg, int len, int pos, cpUcs4* out) {
	cpUcs4 v1, v2, v3, v4, x;
	int ret;
	if (NULL == msg || len < 0 || (unsigned) pos >= (unsigned) len) {
		return -__LINE__;
	}
	if (!out) out = &x;
	ret = 0;
	if (msg[pos] < 0x80) {
		*out = msg[pos];
		return 0;
	} else if (msg[pos] < 0xC0) {
		if (0 == pos) return -__LINE__;
		ret = -__LINE__;
		if (msg[pos-1] >= 0xC1 && msg[pos-1] < 0xF8) {
			pos--;
			ret = 1;
		} else {
			if (1 == pos) return -__LINE__;
			if ((msg[pos-1] | 0x3F) != 0xBF) return -__LINE__;
			if (msg[pos-2] >= 0xE0 && msg[pos-2] < 0xF8) {
				pos -= 2;
				ret = 2;
			} else {
				if (2 == pos) return -__LINE__;
				if ((msg[pos-2] | 0x3F) != 0xBF) return -__LINE__;
				if ((msg[pos-3]|0x07) == 0xF7) {
					pos -= 3;
					ret = 3;
				} else return -__LINE__;
			}
		}
	}
	if (msg[pos] < 0xE0) {
		if (pos + 1 >= len) return -__LINE__;
		v1 = msg[pos]   & ~0xE0;
		v2 = msg[pos+1] & ~0xC0;
		v1 = (v1 << 6) + v2;
		if (v1 < 0x80) return -__LINE__;
		*out = v1;
		return ret;
	}
	if (msg[pos] < 0xF0) {
		if (pos + 2 >= len) return -__LINE__;
		v1 = msg[pos]   & ~0xF0;
		v2 = msg[pos+1] & ~0xC0;
		v3 = msg[pos+2] & ~0xC0;
		v1 = (v1 << 12) + (v2 << 6) + v3;
		if (v1 < 0x800) return -__LINE__;
		if (!isLegalUnicodeCodePoint(v1)) return -__LINE__;
		*out = v1;
		return ret;
	}

	if (msg[pos] >= 0xF8) return -__LINE__;

	if (pos + 3 >= len) return -__LINE__;
	v1 = msg[pos]   & ~0xF8;
	v2 = msg[pos+1] & ~0xC0;
	v3 = msg[pos+2] & ~0xC0;
	v4 = msg[pos+3] & ~0xC0;
	v1 = (v1 << 18) + (v2 << 12) + (v3 << 6) + v4;
	if (v1 < 0x10000) return -__LINE__;
	if (!isLegalUnicodeCodePoint(v1)) return -__LINE__;
	*out = v1;
	return ret;
}

/*
Code point                UTF-8
----------                -----
U-00000000 - U-0000007F:  0xxxxxxx
U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
*/

/*
 *  Returns next read code point for iterator.
 *
 *  iter->data + iter->start points at the characters just read.
 *
 *  iter->data + iter->next points at the characters that will be read next.
 *
 *  iter->error is boolean indicating whether or not last read contained an error.
 */
cpUcs4 utf8IteratorGetNextCodePoint (struct utf8Iterator* iter, cpUcs4 errCh) {
	unsigned char * chrs;
	unsigned char c, d, e;
	long v;
	int i, ofs;

	if (NULL == iter || iter->next < 0) return errCh;
	if (iter->next >= iter->slen) {
		iter->start = iter->slen;
		return errCh;
	}
	if (NULL == iter->data || iter->next < 0 || utf8IteratorNoMore(iter)) return errCh;
	chrs = iter->data + iter->next;

	iter->error = 0;
	c = chrs[0];
	ofs = 0;

	if (c < 0xC0 || c > 0xFD) {
		if (c >= 0x80) goto ErrMode;
		v = c;
		ofs = 1;
	} else if (c < 0xE0) {
		if (iter->next >= iter->slen + 1) goto ErrMode;
		v = (c << 6u) - (0x0C0 << 6u);
		c = (unsigned char) ((unsigned) chrs[1] - 0x080);
		v += c;
		if (c >= 0x40 || v < 0x80) goto ErrMode;
		ofs = 2;
	} else if (c < 0xF0) {
		if (iter->next >= iter->slen + 2) goto ErrMode;
		v = (c << 12) - (0x0E0 << 12u);
		c = (unsigned char) ((unsigned) chrs[1] - 0x080);
		d = (unsigned char) ((unsigned) chrs[2] - 0x080);
		v += (c << 6u) + d;
		if ((c|d) >= 0x40 || v < 0x800 || !isLegalUnicodeCodePoint (v)) goto ErrMode;
		ofs = 3;
	} else if (c < 0xF8) {
		if (iter->next >= iter->slen + 3) goto ErrMode;
		v = (c << 18) - (0x0F0 << 18u);
		c = (unsigned char) ((unsigned) chrs[1] - 0x080);
		d = (unsigned char) ((unsigned) chrs[2] - 0x080);
		e = (unsigned char) ((unsigned) chrs[3] - 0x080);
		v += (c << 12u) + (d << 6u) + e;
		if ((c|d|e) >= 0x40 || v < 0x10000 || !isLegalUnicodeCodePoint (v)) goto ErrMode;
		ofs = 4;
	} else { /* 5 and 6 byte encodings are invalid */
	ErrMode:;
		iter->error = 1;
		v = errCh;
		for (i = iter->next+1; i < iter->slen; i++) if ((iter->data[i] & 0xC0) != 0x80) break;
		ofs = i - iter->next;
	}

	iter->start = iter->next;
	iter->next += ofs;
	return v;
}

/*
 *  Returns next read code point for iterator.
 *
 *  iter->data + iter->start points at the characters to be read.
 *
 *  iter->data + iter->next points at the characters that will be read next.
 *
 *  iter->error is boolean indicating whether or not last read contained an error.
 */
cpUcs4 utf8IteratorGetCurrCodePoint (struct utf8Iterator* iter, cpUcs4 errCh) {
	unsigned char * chrs;
	unsigned char c, d, e;
	long v;

	if (NULL == iter || iter->next < 0) return errCh;
	if (iter->next >= iter->slen) {
		iter->start = iter->slen;
		return errCh;
	}
	if (NULL == iter->data || iter->next < 0 || utf8IteratorNoMore(iter)) return errCh;
	chrs = iter->data + iter->next;

	iter->error = 0;
	c = chrs[0];

	if (c < 0xC0 || c > 0xFD) {
		if (c >= 0x80) goto ErrMode;
		v = c;
	} else if (c < 0xE0) {
		if (iter->next >= iter->slen + 1) goto ErrMode;
		v = (c << 6u) - (0x0C0 << 6u);
		c = (unsigned char) ((unsigned) chrs[1] - 0x080);
		v += c;
		if (c >= 0x40 || v < 0x80) goto ErrMode;
	} else if (c < 0xF0) {
		if (iter->next >= iter->slen + 2) goto ErrMode;
		v = (c << 12lu) - (0x0E0 << 12u);
		c = (unsigned char) ((unsigned) chrs[1] - 0x080);
		d = (unsigned char) ((unsigned) chrs[2] - 0x080);
		v += (c << 6u) + d;
		if ((c|d) >= 0x40 || v < 0x800 || !isLegalUnicodeCodePoint (v)) goto ErrMode;
	} else if (c < 0xF8) {
		if (iter->next >= iter->slen + 3) goto ErrMode;
		v = (c << 18lu) - (0x0F0 << 18u);
		c = (unsigned char) ((unsigned) chrs[1] - 0x080);
		d = (unsigned char) ((unsigned) chrs[2] - 0x080);
		e = (unsigned char) ((unsigned) chrs[3] - 0x080);
		v += (c << 12lu) + (d << 6u) + e;
		if ((c|d|e) >= 0x40 || v < 0x10000 || !isLegalUnicodeCodePoint (v)) goto ErrMode;
	} else { /* 5 and 6 byte encodings are invalid */
	ErrMode:;
		iter->error = 1;
		v = errCh;
	}
	return v;
}
