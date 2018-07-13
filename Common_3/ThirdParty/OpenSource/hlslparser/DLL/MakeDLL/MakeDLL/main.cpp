#include "..\..\..\src\Parser.h"
#include <Objbase.h>

extern "C" __declspec(dllexport) char* PARSER(const char* fileName, char* buffer, int bufferSize, const char* entryName, const char* shader, char* _language)
{	
	Parser parser;

	const char* result = parser.ParserEntry(fileName, buffer, bufferSize, entryName, shader, _language);

	unsigned long ulSize = strlen(result) + sizeof(char);
	char* pszReturn = NULL;
	pszReturn = (char*)::CoTaskMemAlloc(ulSize);
	strcpy(pszReturn, result);
	return  pszReturn;
}