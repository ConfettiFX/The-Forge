/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002-2015, and is covered by the BSD open source
 * license and the GPL. Refer to the accompanying documentation for details
 * on usage and license.
 */

/*
 * buniutil.h
 *
 * This file is the interface for the buniutil basic "Unicode for bstrings"
 * functions.  Note that there are dependencies on bstrlib.h and utf8util.h .
 */

#ifndef BSTRLIB_UNICODE_UTILITIES
#define BSTRLIB_UNICODE_UTILITIES

#include "utf8util.h"
#include "bstrlib.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int buIsUTF8Content (const_bstring bu);
extern int buAppendBlkUcs4 (bstring b, const cpUcs4* bu, int len, cpUcs4 errCh);

/* For those unfortunate enough to be stuck supporting UTF16. */
extern int buGetBlkUTF16 (/* @out */ cpUcs2* ucs2, int len, cpUcs4 errCh, const_bstring bu, int pos);
extern int buAppendBlkUTF16 (bstring bu, const cpUcs2* utf16, int len, cpUcs2* bom, cpUcs4 errCh);

#ifdef __cplusplus
}
#endif

#endif /* BSTRLIB_UNICODE_UTILITIES */

