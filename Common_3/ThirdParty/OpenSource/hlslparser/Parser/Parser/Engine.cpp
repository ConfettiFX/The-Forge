
#include "Engine.h"

#include <stdio.h>  // vsnprintf
#include <string.h> // strcmp, strcasecmp
#include <stdlib.h>	// strtod, strtol
#include <assert.h>

void PARSER_ENGINE_ASSERT()
{
	assert(0);
}

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





// reverses a string 'str' of length 'len'
void reverse(char *str, int len)
{
	int i = 0, j = len - 1, temp;
	while (i<j)
	{
		temp = str[i];
		str[i] = str[j];
		str[j] = temp;
		i++; j--;
	}
}

// https://www.geeksforgeeks.org/convert-floating-point-number-string/
// Converts a given integer x to string str[].  d is the number
// of digits required in output. If d is more than the number
// of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
	int i = 0;
	while (x)
	{
		str[i++] = (x % 10) + '0';
		x = x / 10;
	}

	// If number of digits required is more, then
	// add 0s at the beginning
	while (i < d)
		str[i++] = '0';

	reverse(str, i);
	str[i] = '\0';
	return i;
}

// Converts a floating point number to string.
void ftoa(float n, char *res)
{
	// Extract integer part
	int ipart = (int)n;

	// Extract floating part
	float fpart = n - (float)ipart;

	// convert integer part to string
	int i = intToStr(ipart, res, 0);

	

	int afterpoint = 0;
	//from back, remove serial 0s

	float fcopy = fpart;

	while ((float)((int)fcopy) != fcopy)
	{
		fcopy *= 10.0;
		afterpoint++;
	}

	// check for display option after point
	if (afterpoint != 0)
	{
		if (i == 0)
		{
			res[i++] = '0';
		}

		res[i] = '.';  // add dot

					   // Get the value of fraction part upto given no.
					   // of points after dot. The third parameter is needed
					   // to handle cases like 233.007
		fpart = fpart * (float)pow(10, afterpoint);

		intToStr((int)fpart, res + i + 1, afterpoint);
	}
	else if( strlen(res) == 0 )
	{
		// 0.0
		res[0] = '0';
		res[1] = '.';
		res[2] = '0';
		res[3] = '\0';
	}
	else
	{
		// add .0
		res[i] = '.';
		res[i + 1] = '0';
		res[i + 2] = '\0';

	}
}





int String_PrintfNew(char * buffer, int size, const char * format, float value) {

	ftoa(value, buffer);
	return 1;
}


int String_FormatFloat(char * buffer, int size, float value) {

	if ( value != 0.0  &&  (abs(value) < 1.0e-6 || abs(value) > 1.0e+6))	
		return String_Printf(buffer, size, "%e", value);
	else
		return String_PrintfNew(buffer, size, "%f", value);
}

int String_FormatHalf(char * buffer, int size, float value) {

	return String_PrintfNew(buffer, size, "%f", value);
}

int String_FormatMin16Float(char * buffer, int size, float value)
{
	return String_PrintfNew(buffer, size, "%f", value);
}

int String_FormatMin10Float(char * buffer, int size, float value)
{
	return String_PrintfNew(buffer, size, "%f", value);
}

bool String_Equal(const char * a, const char * b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
	return strcmp(a, b) == 0;
}

bool String_Equal(const eastl::string & a, const char * b)
{
	return String_Equal(a.c_str(),b);
}

bool String_Equal(const char * b, const eastl::string & a)
{
	return String_Equal(a.c_str(), b);
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


char* stristr(const char* str1, const char* str2)
{
	const char* p1 = str1;
	const char* p2 = str2;
	const char* r = *p2 == 0 ? str1 : 0;

	while (*p1 != 0 && *p2 != 0)
	{
		if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2))
		{
			if (r == 0)
			{
				r = p1;
			}

			p2++;
		}
		else
		{
			p2 = str2;
			if (r != 0)
			{
				p1 = r + 1;
			}

			if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2))
			{
				r = p1;
				p2++;
			}
			else
			{
				r = 0;
			}
		}

		p1++;
	}

	return *p2 == 0 ? (char*)r : 0;
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


