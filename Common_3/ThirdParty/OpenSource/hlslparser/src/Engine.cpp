
#include "Engine.h"

#include <stdio.h>  // vsnprintf
#include <string.h> // strcmp, strcasecmp
#include <stdlib.h>	// strtod, strtol




// Engine/String.cpp

int String_PrintfArgList(char * buffer, int size, const char * format, va_list args) {

    va_list tmp;
    va_copy(tmp, args);

#if _MSC_VER >= 1400
	int n = vsnprintf_s(buffer, size, _TRUNCATE, format, tmp);
#else
	int n = vsnprintf(buffer, size, format, tmp);
#endif

    va_end(tmp);

	if (n < 0 || n > size) return -1;
	return n;
}

int String_Printf(char * buffer, int size, const char * format, ...) {

    va_list args;
    va_start(args, format);

    int n = String_PrintfArgList(buffer, size, format, args);

    va_end(args);

	return n;
}

int String_FormatFloat(char * buffer, int size, float value) {

	if ( value != 0.0  &&  (abs(value) < 1.0e-6 || abs(value) > 1.0e+6))	
		return String_Printf(buffer, size, "%e", value);
	else
	 return String_Printf(buffer, size, "%f", value);
}

bool String_Equal(const char * a, const char * b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
	return strcmp(a, b) == 0;
}

bool String_EqualNoCase(const char * a, const char * b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
#if _MSC_VER
	return _stricmp(a, b) == 0;
#else
	return strcasecmp(a, b) == 0;
#endif
}

double String_ToDouble(const char * str, char ** endptr) {
	return strtod(str, endptr);
}

int String_ToInteger(const char * str, char ** endptr) {
	return strtol(str, endptr, 10);
}

unsigned int String_ToUInteger(char * str, char ** endptr) {

	char *x;
	for (x = str; *x; x++) {
		if(!(str[0] >= '0' &&  str[0] <= '9'))
			return 0L;
	}
	return (strtoul(str, 0L, 10));

	//return strtol(str, endptr, 10);
}

int String_ToIntegerHex(const char * str, char ** endptr) {
	return strtol(str, endptr, 16);
}



// Engine/Log.cpp

void Log_Error(const char * format, ...) {
    va_list args;
    va_start(args, format);
    Log_ErrorArgList(format, args);
    va_end(args);
}

void Log_ErrorArgList(const char * format, va_list args) {
#if 1 // @@ Don't we need to do this?
    va_list tmp;
    va_copy(tmp, args);
    vprintf( format, args );
    va_end(tmp);
#else
    vprintf( format, args );
#endif
}


// Engine/StringPool.cpp

StringPool::StringPool(Allocator * allocator) : stringArray(allocator) {
}
StringPool::~StringPool() {
    for (int i = 0; i < stringArray.GetSize(); i++) {
        free((void *)stringArray[i]);
        stringArray[i] = NULL;
    }
}

const char * StringPool::AddString(const char * string) {
    for (int i = 0; i < stringArray.GetSize(); i++) {
        if (String_Equal(stringArray[i], string)) return stringArray[i];
    }
#if _MSC_VER
    const char * dup = _strdup(string);
#else
    const char * dup = strdup(string);
#endif
    stringArray.PushBack(dup);
    return dup;
}

// @@ From mprintf.cpp
static char *mprintf_valist(int size, const char *fmt, va_list args) {
    ASSERT(size > 0);
    char *res = NULL;
    va_list tmp;

    while (1) {
        res = new char[size];
        if (!res) return NULL;

        va_copy(tmp, args);
        int len = vsnprintf(res, size, fmt, tmp);
        va_end(tmp);

        if ((len >= 0) && (size >= len + 1)) {
            break;
        }

        delete [] res;

        if (len > -1 ) {
            size = len + 1;
        }
        else {
            size *= 2;
        }
    }

    return res;
}

const char * StringPool::AddStringFormatList(const char * format, va_list args) {
    va_list tmp;
    va_copy(tmp, args);
    const char * string = mprintf_valist(256, format, tmp);
    va_end(tmp);

    for (int i = 0; i < stringArray.GetSize(); i++) {
        if (String_Equal(stringArray[i], string)) {
            delete [] string;
            return stringArray[i];
        }
    }

    stringArray.PushBack(string);
    return string;
}

const char * StringPool::AddStringFormat(const char * format, ...) {
    va_list args;
    va_start(args, format);
    const char * string = AddStringFormatList(format, args);
    va_end(args);

    return string;
}

bool StringPool::GetContainsString(const char * string) const {
    for (int i = 0; i < stringArray.GetSize(); i++) {
        if (String_Equal(stringArray[i], string)) return true;
    }
    return false;
}

