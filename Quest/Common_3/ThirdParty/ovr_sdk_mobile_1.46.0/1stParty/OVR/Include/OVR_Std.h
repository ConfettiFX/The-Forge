/************************************************************************************

Filename    :   OVR_Std.h
Content     :   Standard C function interface
Created     :   September 19, 2012
Notes       :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Std_h
#define OVR_Std_h

#include "OVR_Types.h"
#include <stdarg.h> // for va_list args
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if !defined(OVR_OS_WINCE) && defined(OVR_CC_MSVC) && (OVR_CC_MSVC >= 1400)
#define OVR_MSVC_SAFESTRING
#include <errno.h>
#endif

// Wide-char funcs
#include <wchar.h>
#include <wctype.h>

namespace OVR {

// Has the same behavior as itoa aside from also having a dest size argument.
// Return value: Pointer to the resulting null-terminated string, same as parameter str.
#if defined(OVR_OS_WIN32)
inline char* OVR_CDECL OVR_itoa(int val, char* dest, size_t destsize, int radix) {
#if defined(OVR_MSVC_SAFESTRING)
    _itoa_s(val, dest, destsize, radix);
    return dest;
#else
    OVR_UNUSED(destsize);
    return itoa(val, dest, radix);
#endif
}
#else // OVR_OS_WIN32
inline char* OVR_itoa(int val, char* dest, size_t len, int radix) {
    if (val == 0) {
        if (len > 1) {
            dest[0] = '0';
            dest[1] = '\0';
        }
        return dest;
    }

    // FIXME: Fix the following code to avoid memory write overruns when len is in sufficient.
    int cur = val;
    size_t i = 0;
    size_t sign = 0;

    if (val < 0) {
        val = -val;
        sign = 1;
    }

    while ((val != 0) && (i < (len - 1 - sign))) {
        cur = val % radix;
        val /= radix;

        if (radix == 16) {
            switch (cur) {
                case 10:
                    dest[i] = 'a';
                    break;
                case 11:
                    dest[i] = 'b';
                    break;
                case 12:
                    dest[i] = 'c';
                    break;
                case 13:
                    dest[i] = 'd';
                    break;
                case 14:
                    dest[i] = 'e';
                    break;
                case 15:
                    dest[i] = 'f';
                    break;
                default:
                    dest[i] = (char)('0' + cur);
                    break;
            }
        } else {
            dest[i] = (char)('0' + cur);
        }
        ++i;
    }

    if (sign) {
        dest[i++] = '-';
    }

    for (size_t j = 0; j < i / 2; ++j) {
        char tmp = dest[j];
        dest[j] = dest[i - 1 - j];
        dest[i - 1 - j] = tmp;
    }
    dest[i] = '\0';

    return dest;
}

#endif

// String functions

inline size_t OVR_CDECL OVR_strlen(const char* str) {
    return strlen(str);
}

inline char* OVR_CDECL OVR_strcpy(char* dest, size_t destsize, const char* src) {
#if defined(OVR_MSVC_SAFESTRING)
    strcpy_s(dest, destsize, src);
    return dest;
#elif defined(OVR_OS_ANDROID)
    strlcpy(dest, src, destsize);
    return dest;
#else
    // FIXME: This should be a safer implementation
    OVR_UNUSED(destsize);
    return strcpy(dest, src);
#endif
}

inline char* OVR_CDECL OVR_strncpy(char* dest, size_t destsize, const char* src, size_t count) {
    // handle an invalid destination the same way for all cases
    if (destsize == 0 || dest == NULL || src == NULL) {
        OVR_ASSERT(dest != NULL && destsize > 0 && src != NULL);
        return dest;
    }
    if (dest == src) {
        return dest;
    }
    // special case if count is 0, just set the first character to null byte
    if (count == 0) {
        dest[0] = '\0';
        return dest;
    }

    if (count >= destsize) {
        // if count is larger than or equal to the destination buffer size, completely fill the
        // destination buffer and put the zero byte at the end
#if defined(OVR_MSVC_SAFESTRING)
        strncpy_s(dest, destsize, src, destsize);
#else
        strncpy(dest, src, destsize);
#endif
        dest[destsize - 1] = '\0';
    } else {
        // otherwise place it right after the count
#if defined(OVR_MSVC_SAFESTRING)
        strncpy_s(dest, destsize, src, count);
#else
        strncpy(dest, src, count);
#endif
        dest[count] = '\0';
    }
    return dest;
}

inline char* OVR_CDECL OVR_strcat(char* dest, size_t destsize, const char* src) {
#if defined(OVR_MSVC_SAFESTRING)
    strcat_s(dest, destsize, src);
    return dest;
#elif defined(OVR_OS_ANDROID)
    strlcat(dest, src, destsize);
    return dest;
#else
    // FIXME: This should be a safer implementation
    OVR_UNUSED(destsize);
    return strcat(dest, src);
#endif
}

inline int OVR_CDECL OVR_strcmp(const char* dest, const char* src) {
    return strcmp(dest, src);
}

inline const char* OVR_CDECL OVR_strchr(const char* str, char c) {
    return strchr(str, c);
}

inline char* OVR_CDECL OVR_strchr(char* str, char c) {
    return strchr(str, c);
}

inline const char* OVR_strrchr(const char* str, char c) {
    size_t len = OVR_strlen(str);
    for (size_t i = len; i > 0; i--)
        if (str[i] == c)
            return str + i;
    return 0;
}

inline const uint8_t* OVR_CDECL OVR_memrchr(const uint8_t* str, size_t size, uint8_t c) {
    for (intptr_t i = (intptr_t)size - 1; i >= 0; i--) {
        if (str[i] == c)
            return str + i;
    }
    return 0;
}

inline char* OVR_CDECL OVR_strrchr(char* str, char c) {
    size_t len = OVR_strlen(str);
    for (size_t i = len; i > 0; i--)
        if (str[i] == c)
            return str + i;
    return 0;
}

double OVR_CDECL OVR_strtod(const char* string, char** tailptr);

inline long OVR_CDECL OVR_strtol(const char* string, char** tailptr, int radix) {
    return strtol(string, tailptr, radix);
}

inline unsigned long OVR_CDECL OVR_strtoul(const char* string, char** tailptr, int radix) {
    return strtoul(string, tailptr, radix);
}

inline int OVR_CDECL OVR_strncmp(const char* ws1, const char* ws2, size_t size) {
    return strncmp(ws1, ws2, size);
}

inline uint64_t OVR_CDECL OVR_strtouq(const char* nptr, char** endptr, int base) {
#if defined(OVR_CC_MSVC) && !defined(OVR_OS_WINCE)
    return _strtoui64(nptr, endptr, base);
#else
    return strtoull(nptr, endptr, base);
#endif
}

inline int64_t OVR_CDECL OVR_strtoq(const char* nptr, char** endptr, int base) {
#if defined(OVR_CC_MSVC) && !defined(OVR_OS_WINCE)
    return _strtoi64(nptr, endptr, base);
#else
    return strtoll(nptr, endptr, base);
#endif
}

inline int64_t OVR_CDECL OVR_atoq(const char* string) {
#if defined(OVR_CC_MSVC) && !defined(OVR_OS_WINCE)
    return _atoi64(string);
#else
    return atoll(string);
#endif
}

inline uint64_t OVR_CDECL OVR_atouq(const char* string) {
    return OVR_strtouq(string, NULL, 10);
}

// Case insensitive compare implemented in platform-specific way.
inline int OVR_CDECL OVR_stricmp(const char* a, const char* b) {
#if defined(OVR_OS_WIN32)
#if defined(OVR_CC_MSVC) && (OVR_CC_MSVC >= 1400)
    return ::_stricmp(a, b);
#else
    return ::stricmp(a, b);
#endif

#else
    return strcasecmp(a, b);
#endif
}

inline int OVR_CDECL OVR_strnicmp(const char* a, const char* b, size_t count) {
#if defined(OVR_OS_WIN32)
#if defined(OVR_CC_MSVC) && (OVR_CC_MSVC >= 1400)
    return ::_strnicmp(a, b, count);
#else
    return ::strnicmp(a, b, count);
#endif

#else
    return strncasecmp(a, b, count);
#endif
}

// OVR_strdup
// Safe for Windows without turning off deprecation warnings. MSVC suggests _strdup
// but that function only exists in MSVC.
// Always 0-terminates.
// Returns an empty string if NULL is passed -- for normal strdup that is undefined behavior.
inline char* OVR_CDECL OVR_strdup(const char* str) {
    if (str == NULL) {
        // for POSIX strdup, str == NULL is undefined.
        // assert, and just allocate a small buffer and make it empty-string
        OVR_ASSERT(str != NULL);
        char* result = static_cast<char*>(malloc(8));
        if (result == NULL) {
            return NULL;
        }
        result[0] = '\0';
        return result;
    }

    const size_t size = OVR_strlen(str) + 1;

    char* result = static_cast<char*>(malloc(size));
    if (result == NULL) {
        return NULL;
    }

    // copy the input string -- OVR_strcpy always 0-terminates
    OVR_strcpy(result, size, str);

    return result;
}

inline int OVR_CDECL OVR_sprintf(char* dest, size_t destsize, const char* format, ...) {
    if (destsize <= 0 || dest == NULL) {
        OVR_ASSERT(destsize > 0);
        return -1;
    }
    va_list argList;
    va_start(argList, format);
    int ret;
#if defined(OVR_CC_MSVC)
#if defined(OVR_MSVC_SAFESTRING)
    ret = _vsnprintf_s(dest, destsize, _TRUNCATE, format, argList);
#else
    // FIXME: this is a security issue on Windows platforms that don't have _vsnprintf_s
    OVR_UNUSED(destsize);
    ret = _vsnprintf(dest, destsize - 1, format, argList); // -1 for space for the null character
    dest[destsize - 1] = 0; // may leave trash in the destination...
#endif
#else
    ret = vsnprintf(dest, destsize, format, argList);
    // In the event of the output string being greater than the buffer size, vsnprintf should
    // return the size of the string before truncation. In that case we zero-terminate the
    // string to ensure that the result is the same as _vsnprintf_s would return for the
    // MSVC compiler. vsnprintf is supposed to zero-terminate in this case, but to be sure
    // we zero-terminate it ourselves.
    if (ret >= (int)destsize) {
        dest[destsize - 1] = '\0';
    }
#endif
    // If an error occurs, vsnprintf should return -1, in which case we set zero byte to null
    // character.
    OVR_ASSERT(ret >= 0); // ensure the format string is not malformed
    if (ret < 0) {
        dest[0] = '\0';
    }
    va_end(argList);
    return ret;
}

// Returns the number of characters that were written to the destination buffer. If the formatted
// text fits in the destination buffer, then this value is < destsize.
// The size of the destination buffer is returned if either of the following is true:
// - a formatting error occurs
// - the formatted text does not fit in the destination buffer.
// This does not mirror the Windows vsnprintf functionality OR the Linux functionality.
// Normally in an overflow case, Linux will return the number of bytes that would have been written
// if the buffer were large enough to fit the formatted text. This allows a caller to reallocate a
// buffer of the necessary size and try again. Windows functions, however, do not return the length
// that would have been written to the buffer, so we have no simple way to make that Linux behavior
// portable to Windows.
// Thus, in the event of an overflow we simply return destsize to indicate an overflow.
// An alternative would be to return destsize - 1, but that would rule out returning any overflow
// indicator without changing the function signature.
inline size_t OVR_CDECL
OVR_vsprintf(char* dest, size_t destsize, const char* format, va_list argList) {
    OVR_ASSERT(dest != NULL);

    // always just 0-terminate the dest
    dest[0] = '\0';

#if defined(OVR_CC_MSVC)
#if defined(OVR_MSVC_SAFESTRING)
    int bytesWritten = vsnprintf_s(dest, destsize, _TRUNCATE, format, argList);
#else
    OVR_UNUSED(destsize);
    // for vsnprintf, the second parameters is the maximum number of characters to write, meaning
    // NOT the size of the buffer, but the size of the buffer minus 1 for the NULL character.
    int bytesWritten = _vsnprintf(dest, destsize - 1, format, argList);
#endif
#else
    // specifically for Android / Linux
    int bytesWritten = vsnprintf(dest, destsize, format, argList);
#endif
    // Windows: for vsnprintf and vsnprintf_s with _TRUNCATE, a return of -1 means the buffer was
    // overflowed Android / Linux: -1 is only returned if a format error occured, bytesWritten ==
    // destsize means buffer overflow
    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) >= destsize) {
        OVR_ASSERT(bytesWritten >= 0);
        // zero terminate at the very end
        dest[destsize - 1] = '\0';
        // return the size of the buffer to indecate an overflow
        return static_cast<size_t>(destsize);
    } else {
        OVR_ASSERT(bytesWritten >= 0 && static_cast<size_t>(bytesWritten) < destsize);
        // ensure zero-terminator after the written bytes
        dest[bytesWritten] = '\0';
        return static_cast<size_t>(bytesWritten);
    }
}

// Returns the strlen of the resulting formatted string, or a negative value if the format is
// invalid. Note: If you are planning on printing a string then it's more efficient to just use
// OVR_vsnprintf and look at the return value and handle the uncommon case that there wasn't enough
// space.
inline int OVR_CDECL OVR_vscprintf(const char* format, va_list argList) {
    int ret;
#if defined(OVR_CC_MSVC)
    ret = _vscprintf(format, argList);
#else
    ret = vsnprintf(NULL, 0, format, argList);
#endif
    return ret;
}

wchar_t* OVR_CDECL OVR_wcscpy(wchar_t* dest, size_t destsize, const wchar_t* src);
wchar_t* OVR_CDECL OVR_wcsncpy(wchar_t* dest, size_t destsize, const wchar_t* src, size_t count);
wchar_t* OVR_CDECL OVR_wcscat(wchar_t* dest, size_t destsize, const wchar_t* src);
size_t OVR_CDECL OVR_wcslen(const wchar_t* str);
int OVR_CDECL OVR_wcscmp(const wchar_t* a, const wchar_t* b);
int OVR_CDECL OVR_wcsicmp(const wchar_t* a, const wchar_t* b);

inline int OVR_CDECL OVR_wcsicoll(const wchar_t* a, const wchar_t* b) {
#if defined(OVR_OS_WIN32)
#if defined(OVR_CC_MSVC) && (OVR_CC_MSVC >= 1400)
    return ::_wcsicoll(a, b);
#else
    return ::wcsicoll(a, b);
#endif
#else
    // not supported, use regular wcsicmp
    return OVR_wcsicmp(a, b);
#endif
}

inline int OVR_CDECL OVR_wcscoll(const wchar_t* a, const wchar_t* b) {
#if defined(OVR_OS_WIN32) || defined(OVR_OS_LINUX)
    return wcscoll(a, b);
#else
    // not supported, use regular wcscmp
    return OVR_wcscmp(a, b);
#endif
}

#ifndef OVR_NO_WCTYPE

inline int OVR_CDECL UnicodeCharIs(const uint16_t* table, wchar_t charCode) {
    unsigned offset = table[charCode >> 8];
    if (offset == 0)
        return 0;
    if (offset == 1)
        return 1;
    return (table[offset + ((charCode >> 4) & 15)] & (1 << (charCode & 15))) != 0;
}

extern const uint16_t UnicodeAlnumBits[];
extern const uint16_t UnicodeAlphaBits[];
extern const uint16_t UnicodeDigitBits[];
extern const uint16_t UnicodeSpaceBits[];
extern const uint16_t UnicodeXDigitBits[];

// Uncomment if necessary
// extern const uint16_t UnicodeCntrlBits[];
// extern const uint16_t UnicodeGraphBits[];
// extern const uint16_t UnicodeLowerBits[];
// extern const uint16_t UnicodePrintBits[];
// extern const uint16_t UnicodePunctBits[];
// extern const uint16_t UnicodeUpperBits[];

inline int OVR_CDECL OVR_iswalnum(wchar_t charCode) {
    return UnicodeCharIs(UnicodeAlnumBits, charCode);
}
inline int OVR_CDECL OVR_iswalpha(wchar_t charCode) {
    return UnicodeCharIs(UnicodeAlphaBits, charCode);
}
inline int OVR_CDECL OVR_iswdigit(wchar_t charCode) {
    return UnicodeCharIs(UnicodeDigitBits, charCode);
}
inline int OVR_CDECL OVR_iswspace(wchar_t charCode) {
    return UnicodeCharIs(UnicodeSpaceBits, charCode);
}
inline int OVR_CDECL OVR_iswxdigit(wchar_t charCode) {
    return UnicodeCharIs(UnicodeXDigitBits, charCode);
}

// clang-format off
// Uncomment if necessary
//inline int OVR_CDECL OVR_iswcntrl (wchar_t charCode) { return UnicodeCharIs(UnicodeCntrlBits,  charCode); }
//inline int OVR_CDECL OVR_iswgraph (wchar_t charCode) { return UnicodeCharIs(UnicodeGraphBits,  charCode); }
//inline int OVR_CDECL OVR_iswlower (wchar_t charCode) { return UnicodeCharIs(UnicodeLowerBits,  charCode); }
//inline int OVR_CDECL OVR_iswprint (wchar_t charCode) { return UnicodeCharIs(UnicodePrintBits,  charCode); }
//inline int OVR_CDECL OVR_iswpunct (wchar_t charCode) { return UnicodeCharIs(UnicodePunctBits,  charCode); }
//inline int OVR_CDECL OVR_iswupper (wchar_t charCode) { return UnicodeCharIs(UnicodeUpperBits,  charCode); }
// clang-format on

int OVR_CDECL OVR_towupper(wchar_t charCode);
int OVR_CDECL OVR_towlower(wchar_t charCode);

#else // OVR_NO_WCTYPE

inline int OVR_CDECL OVR_iswspace(wchar_t c) {
    return iswspace(c);
}

inline int OVR_CDECL OVR_iswdigit(wchar_t c) {
    return iswdigit(c);
}

inline int OVR_CDECL OVR_iswxdigit(wchar_t c) {
    return iswxdigit(c);
}

inline int OVR_CDECL OVR_iswalpha(wchar_t c) {
    return iswalpha(c);
}

inline int OVR_CDECL OVR_iswalnum(wchar_t c) {
    return iswalnum(c);
}

inline wchar_t OVR_CDECL OVR_towlower(wchar_t c) {
    return (wchar_t)towlower(c);
}

inline wchar_t OVR_towupper(wchar_t c) {
    return (wchar_t)towupper(c);
}

#endif // OVR_NO_WCTYPE

// ASCII versions of tolower and toupper. Don't use "char"
inline int OVR_CDECL OVR_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

inline int OVR_CDECL OVR_toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

inline double OVR_CDECL OVR_wcstod(const wchar_t* string, wchar_t** tailptr) {
#if defined(OVR_OS_OTHER)
    OVR_UNUSED(tailptr);
    char buffer[64];
    char* tp = NULL;
    size_t max = OVR_wcslen(string);
    if (max > 63)
        max = 63;
    unsigned char c = 0;
    for (size_t i = 0; i < max; i++) {
        c = (unsigned char)string[i];
        buffer[i] = ((c) < 128 ? (char)c : '!');
    }
    buffer[max] = 0;
    return OVR_strtod(buffer, &tp);
#else
    return wcstod(string, tailptr);
#endif
}

inline long OVR_CDECL OVR_wcstol(const wchar_t* string, wchar_t** tailptr, int radix) {
#if defined(OVR_OS_OTHER)
    OVR_UNUSED(tailptr);
    char buffer[64];
    char* tp = NULL;
    size_t max = OVR_wcslen(string);
    if (max > 63)
        max = 63;
    unsigned char c = 0;
    for (size_t i = 0; i < max; i++) {
        c = (unsigned char)string[i];
        buffer[i] = ((c) < 128 ? (char)c : '!');
    }
    buffer[max] = 0;
    return strtol(buffer, &tp, radix);
#else
    return wcstol(string, tailptr, radix);
#endif
}

} // namespace OVR

#endif // OVR_Std_h
