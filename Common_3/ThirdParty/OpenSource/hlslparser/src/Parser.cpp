#include "Parser.h"

	enum Target
	{
		Target_VertexShader,
		Target_FragmentShader,
		Target_HullShader,
		Target_DomainShader,
		Target_GeometryShader,
		Target_ComputeShader,
	};

	enum Language
	{
		Language_GLSL,
		Language_HLSL,
		Language_LegacyHLSL,
		Language_Metal,
	};

	

	void Parser::PrintUsage()
	{
		std::cerr << "usage: hlslparser [-h] [-fs | -vs] FILENAME ENTRYNAME\n"
			<< "\n"
			<< "Translate HLSL shader to GLSL shader.\n"
			<< "\n"
			<< "positional arguments:\n"
			<< " FILENAME    input file name\n"
			<< " ENTRYNAME   entry point of the shader\n"
			<< "\n"
			<< "optional arguments:\n"
			<< " -h, --help  show this help message and exit\n"
			<< " -fs         generate fragment shader (default)\n"
			<< " -vs         generate vertex shader\n"
			<< " -glsl       generate GLSL (default)\n"
			<< " -hlsl       generate HLSL\n"
			<< " -legacyhlsl generate legacy HLSL\n"
			<< " -metal      generate MSL\n";
	}

	const char* Parser::ParserEntry(const char* fileName, const char* buffer, int bufferSize, const char* entryName, const char* shader, const char* _language)
	{
		//char temp[64];
		//strcpy(temp, "");
		//return NULL;
		//using namespace M4;
		
		Log_Error("In ParserEntry...\n");

		Target target = Target_VertexShader;
		Language language = Language_GLSL;

		char RESULT[65536];
		memset(RESULT, NULL, sizeof(char)*65536);

		if (String_Equal(shader, "-h") || String_Equal(shader, "--help"))
		{
			PrintUsage();
		}
		else if (String_Equal(shader, "-fs"))
		{
			target = Target_FragmentShader;
		}
		else if (String_Equal(shader, "-vs"))
		{
			target = Target_VertexShader;
		}
		else if (String_Equal(shader, "-hs"))
		{
			target = Target_HullShader;
		}
		else if (String_Equal(shader, "-ds"))
		{
			target = Target_DomainShader;
		}
		else if (String_Equal(shader, "-gs"))
		{
			target = Target_GeometryShader;
		}
		else if (String_Equal(shader, "-cs"))
		{
			target = Target_ComputeShader;
		}

		if (String_Equal(_language, "-glsl"))
		{
			language = Language_GLSL;
		}
		else if (String_Equal(_language, "-hlsl"))
		{
			language = Language_HLSL;
		}
		else if (String_Equal(_language, "-legacyhlsl"))
		{
			language = Language_LegacyHLSL;
		}
		else if (String_Equal(_language, "-metal"))
		{
			language = Language_Metal;
		}
		else
		{
			Log_Error("Too many arguments\n");
			PrintUsage();
			strcpy(RESULT, "Too many arguments\n");
			return RESULT;
		}


		if (fileName == NULL || entryName == NULL)
		{
			Log_Error("Missing arguments\n");
			PrintUsage();
			strcpy(RESULT, "Missing arguments\n");
			return RESULT;
		}

		

		PreprocessorPackage	m_PreprocessorPackage[128];

		Log_Error("Parsing...\n");

		// Parse input file
		Allocator allocator;
		HLSLParser parser(&allocator, fileName, buffer, bufferSize, entryName);
		HLSLTree tree(&allocator);
		if (!parser.Parse(&tree, m_PreprocessorPackage))
		{
			Log_Error("Parsing failed, aborting\n");
			strcpy(RESULT, "Parsing failed, aborting\n");
			return RESULT;
		}

		Log_Error("Translating...\n");

		// Generate output
		if (language == Language_GLSL)
		{
			GLSLGenerator generator;
			if (!generator.Generate(&tree, GLSLGenerator::Target(target), GLSLGenerator::Version_450, entryName))
			{
				Log_Error("Translation failed, aborting\n");
				strcpy(RESULT, "Translation failed, aborting\n");
				return RESULT;
			}

			//std::cout << generator.GetResult();
			strcpy(RESULT, generator.GetResult());
		}
		else if (language == Language_HLSL)
		{
			HLSLGenerator generator;
			if (!generator.Generate(&tree, HLSLGenerator::Target(target), entryName, language == Language_LegacyHLSL))
			{
				Log_Error("Translation failed, aborting\n");
				strcpy(RESULT, "Translation failed, aborting\n");
				return RESULT;
			}

			//std::cout << generator.GetResult();
			strcpy(RESULT, generator.GetResult());
		}
		else if (language == Language_Metal)
		{
			MSLGenerator generator;
			if (!generator.Generate(&tree, MSLGenerator::Target(target), entryName))
			{
				Log_Error("Translation failed, aborting\n");
				strcpy(RESULT, "Translation failed, aborting\n");
				return RESULT;
			}

			//std::cout << generator.GetResult();
			strcpy(RESULT, generator.GetResult());
		}

		Log_Error("Success!");

		return RESULT;
	}



