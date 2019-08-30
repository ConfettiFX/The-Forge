#ifndef ENGINE_H
#define ENGINE_H

#if _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdarg.h> // va_list, vsnprintf
#include <stdlib.h> // malloc
#include <new> // for placement new
#include <ctype.h>

#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"
#include "../../../EASTL/deque.h"


#ifndef NULL
#define NULL    0
#endif

#ifndef va_copy
#define va_copy(a, b) (a) = (b)
#endif

// Engine/Assert.h

void PARSER_ENGINE_ASSERT();

#define ASSERT_PARSER(x) do { if (!(x)) { PARSER_ENGINE_ASSERT(); } } while (0);

#define ASSERT_PARSER_DEBUG(x) do { if (!(x)) { PARSER_ENGINE_ASSERT(); } } while (0);


// Engine/String.h

int String_Printf(char * buffer, int size, const char * format, ...);
int String_PrintfArgList(char * buffer, int size, const char * format, va_list args);
int String_FormatFloat(char * buffer, int size, float value);
int String_FormatHalf(char * buffer, int size, float value);
int String_FormatMin16Float(char * buffer, int size, float value);
int String_FormatMin10Float(char * buffer, int size, float value);

bool String_Equal(const eastl::string & a, const char * b);
bool String_Equal(const char * b, const eastl::string & a);

bool String_Equal(const char * a, const char * b);
bool String_EqualNoCase(const char * a, const char * b);
double String_ToDouble(const char * str, char ** end);
int String_ToInteger(const char * str, char ** end);
unsigned int String_ToUInteger(char * str, char ** end);

char* stristr(const char* str1, const char* str2);


// Engine/Log.h

void Log_Error(const char * format, ...);
void Log_ErrorArgList(const char * format, va_list args);


#endif // ENGINE_H
