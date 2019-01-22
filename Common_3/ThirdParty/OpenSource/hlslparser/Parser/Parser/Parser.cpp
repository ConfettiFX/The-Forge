#include "Parser.h"

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

	const char* Parser::ParserEntry(char* RESULT,  const char* fileName[], const char* buffer, size_t bufferSize, const char* entryName, const char* shader, const char* _language, const char* bufferForInlcuded[], int includedCounter)
	{
		//char temp[64];
		//strcpy(temp, "");
		//return NULL;
		//using namespace M4;
		
		//Log_Error("In ParserEntry...\n");

		Target target = Target_VertexShader;
		Language language = Language_GLSL;

		//char RESULT[65536];
		//memset(RESULT, NULL, sizeof(char)*65536);

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
		else if (String_Equal(_language, "-msl"))
		{
			language = Language_MSL;
		}
		else
		{
			Log_Error("error) : Too many arguments\n");
			PrintUsage();
			strcpy(RESULT, "error) : Too many arguments\n");
			return RESULT;
		}

		if (fileName == NULL || entryName == NULL)
		{
			Log_Error("error) : Missing arguments\n");
			strcpy(RESULT, "error) : Missing arguments\n");
			return RESULT;
		}

		// Parse input file
		Allocator allocator;
		HLSLParser parser(&allocator, fileName, buffer, bufferSize, entryName, target, language, bufferForInlcuded, includedCounter);
		HLSLTree tree(&allocator);
		if (!parser.Parse(&tree))
		{
			Log_Error("error) : Parsing failed, aborting\n");
			
			strcpy(RESULT, parser.mainTokenizer->errorBuffer);
			strcat(RESULT, "\nerror) : Parsing failed, aborting\n");
			return RESULT;
		}




		// Generate output
		if (language == Language_GLSL)
		{
			GLSLGenerator generator;
			if (!generator.Generate(&tree, GLSLGenerator::Target(target), GLSLGenerator::Version_450, entryName))
			{
				Log_Error("error) : Translation failed, aborting\n");
				strcpy(RESULT, "error) : Translation failed, aborting\n");
				return RESULT;
			}
#ifdef _DEBUG
			std::cout << generator.GetResult();
#endif // DEBUG
			
			strcpy(RESULT, generator.GetResult());
		}
		else if (language == Language_HLSL)
		{
			HLSLGenerator generator;
			if (!generator.Generate(&tree, HLSLGenerator::Target(target), entryName, language == Language_LegacyHLSL))
			{
				Log_Error("error) : Translation failed, aborting\n");
				strcpy(RESULT, "error) : Translation failed, aborting\n");
				return RESULT;
			}

#ifdef _DEBUG
			std::cout << generator.GetResult();
#endif // DEBUG
			strcpy(RESULT, generator.GetResult());
		}
		else if (language == Language_MSL)
		{
			if (target == Target_GeometryShader)
			{
				Log_Error("error) : Metal doesn't support Geometry shader\n");
				strcpy(RESULT, "error) : Metal doesn't support Geometry shader\n");
				return RESULT;
			}

			MSLGenerator generator;
			if (!generator.Generate(&tree, MSLGenerator::Target(target), entryName))
			{
				Log_Error("error) : Translation failed, aborting\n");
				strcpy(RESULT, "error) : Translation failed, aborting\n");
				return RESULT;
			}

#ifdef _DEBUG
			std::cout << generator.GetResult();
#endif // DEBUG
			strcpy(RESULT, generator.GetResult());
		}


		return RESULT;
	}