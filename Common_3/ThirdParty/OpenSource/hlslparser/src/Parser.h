#pragma once
#include "HLSLParser.h"

#include "GLSLGenerator.h"
#include "HLSLGenerator.h"
#include "MSLGenerator.h"

#include <fstream>
#include <sstream>
#include <iostream>



	class Parser
	{
	public:

		static const char* ParserEntry(const char* fileName, const char* buffer, int bufferSize, const char* entryName, const char* shader, const char* _language);
		
		static void PrintUsage();
	};



