#include "..\Parser\Parser.h"
#include <Objbase.h>
#include <string.h>

extern "C" __declspec(dllexport) char* PARSER(const char* fileName[], char* buffer, int bufferSize, const char* entryName, const char* shader, char* _language, const char* bufferForInlcuded[], int includedCounter)
{	
	Parser parser;

	char RESULT[65536];
	memset(RESULT, NULL, sizeof(char)*65536);	

	// need to change this
	//parser.ParserEntry(RESULT, fileName, buffer, bufferSize, entryName, shader, _language, bufferForInlcuded, includedCounter);

	unsigned long ulSize = (unsigned long)strlen(RESULT) + (unsigned long)sizeof(char);
	char* pszReturn = NULL;
	pszReturn = (char*)::CoTaskMemAlloc(ulSize);
	strcpy_s(pszReturn, ulSize,RESULT);
	return  pszReturn;
}