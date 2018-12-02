#pragma once
#include "HLSLParser.h"

#include "GLSLGenerator.h"
#include "HLSLGenerator.h"
#include "MSLGenerator.h"






	class Parser
	{
	public:

		static const char* ParserEntry(char* RESULT, const char* fileName[], const char* buffer, size_t bufferSize, const char* entryName, const char* shader, const char* _language, const char* bufferForInlcuded[], int includedCounter);
		
		static void PrintUsage();
	};



