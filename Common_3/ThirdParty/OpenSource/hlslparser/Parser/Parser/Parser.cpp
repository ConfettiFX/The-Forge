#include "Parser.h"

#include "HLSLParser.h"
#include "HLSLGenerator.h"
#include "HLSLTokenizer.h"
#include "MSLGenerator.h"
#include "GLSLGenerator.h"
#include "MCPPPreproc.h"

#include <direct.h>

#pragma warning(disable:4996)

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


static eastl::string ReadFile(const char* fileName)
{
	std::ifstream ifs(fileName);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return eastl::string(buffer.str().c_str());
}


static bool WriteFile(const char* fileName, const char* contents)
{
	//check first if there is right folder along the path of filename
	//And make directory	

	size_t found;
	std::string FilenameStr(fileName);
	found = FilenameStr.find_last_of("/\\");
	std::string DirnameStr = FilenameStr.substr(0, found);

	_mkdir(DirnameStr.c_str());



	std::ofstream ofs(fileName);

	ofs << contents;
	ofs.close();
	return true;
}

bool Parser::ProcessFile(
	ParsedData & parsedData,
	const eastl::string & srcFileName,
	const eastl::string & entryName,
	const Options & options,
	const eastl::vector < eastl::string > & macroLhs,
	const eastl::vector < eastl::string > & macroRhs)
{
	ASSERT_PARSER(options.mOperation == Operation_Preproc ||
		options.mOperation == Operation_Parse ||
		options.mOperation == Operation_Generate);

	parsedData.Reset();

	parsedData.mEntry = entryName;
	parsedData.mSrcName = srcFileName;
	parsedData.mMacroLhs = macroLhs;
	parsedData.mMacroRhs = macroRhs;
	parsedData.mOperation = options.mOperation;
	parsedData.mLanguage = options.mLanguage;
	parsedData.mIsSuccess = false;

	// ignore the debug messages
	eastl::string preprocDebug;
	parsedData.mIsPreprocOk = MCPPPreproc::FetchPreProcDataAsString(parsedData.mPreprocData, parsedData.mPreprocErrors, preprocDebug, srcFileName.c_str(), macroLhs, macroRhs);

	if (!parsedData.mIsPreprocOk)
	{
		return false;
	}

	if (options.mDebugPreprocEnable)
	{
		WriteFile(options.mDebugPreprocFile.c_str(), parsedData.mPreprocData.c_str());
	}

	// are we done
	if (options.mOperation == Operation_Preproc)
	{
		parsedData.mIsSuccess = true;
		return true;
	}

	// parse it
	StringLibrary stringLibrary;
	HLSLParser::IntrinsicHelper intrinsicHelper;
	intrinsicHelper.BuildIntrinsics();

	FullTokenizer fullTokenizer(&stringLibrary, srcFileName.c_str(), parsedData.mPreprocData.c_str(), parsedData.mPreprocData.size() + 1);

	HLSLParser parser(&stringLibrary, &intrinsicHelper, &fullTokenizer, entryName.c_str(), options.mTarget, options.mLanguage,
		options.mDebugTokenEnable ? options.mDebugTokenFile.c_str() : NULL);

	HLSLTree tree(&stringLibrary);

	parsedData.mParseErrors = parser.m_pFullTokenizer->errorBuffer;
	parsedData.mIsParseOk = parser.Parse(&tree);
	if (!parsedData.mIsParseOk)
	{
		return false;
	}

	// are we done
	if (options.mOperation == Operation_Parse)
	{
		parsedData.mIsSuccess = true;
		return true;
	}

	// if we got this far, we have to generate code (options.mOperation == Operation_Generate)
	if (options.mLanguage == Language_HLSL) // DX12
	{
		HLSLGenerator generator;
		parsedData.mIsGenerateOk = generator.Generate(&stringLibrary, &tree, (HLSLGenerator::Target)options.mTarget, entryName.c_str(), false);
		parsedData.mGeneratedData = generator.GetResult();
	}
	else if (options.mLanguage == Language_GLSL) // GLSL
	{
		GLSLGenerator generator;
		parsedData.mIsGenerateOk = generator.Generate(&stringLibrary, &tree, (GLSLGenerator::Target)options.mTarget, GLSLGenerator::Version_450, entryName.c_str());
		parsedData.mGeneratedData = generator.GetResult();

		if (!parsedData.mIsGenerateOk)
		{
			parsedData.mIsSuccess = false;
			return false;
		}

	}
	else if (options.mLanguage == Language_MSL) // MSL
	{
		MSLGenerator generator;

		MSLGenerator::Options mslOptions;
		mslOptions.bindingRequired = options.mOverrideRequired;
		mslOptions.bindingOverrides = options.mOverrideVec;

		parsedData.mIsGenerateOk = generator.Generate(&stringLibrary, &tree, (MSLGenerator::Target)options.mTarget, entryName.c_str(), mslOptions);
		parsedData.mGeneratedData = generator.GetResult();

	}
	else
	{
		ASSERT_PARSER(0);
	}

	if (!parsedData.mIsGenerateOk)
	{
		parsedData.mIsSuccess = false;
		return false;
	}

	if (options.mGeneratedWriteEnable)
	{
		WriteFile(options.mGeneratedWriteFile.c_str(), parsedData.mGeneratedData.c_str());
	}

	parsedData.mIsSuccess = true;
	return true;
}
#if 0
const char* Parser::ParserEntry(char* RESULT,  const char* fileName, const char* buffer, int bufferSize, const char* entryName, const char* shader, const char* _language)
{
	Target target = Target_VertexShader;
	Language language = Language_GLSL;

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

	StringLibrary stringLibrary;

	// Parse input file
	HLSLParser::IntrinsicHelper intrinsicHelper;
	intrinsicHelper.BuildIntrinsics();

	FullTokenizer fullTokenizer(&stringLibrary, fileName[0], buffer, bufferSize);

	HLSLParser parser(&stringLibrary, &intrinsicHelper, &fullTokenizer, entryName, target, language, NULL);
	HLSLTree tree(&stringLibrary);
	if (!parser.Parse(&tree))
	{
		Log_Error("error) : Parsing failed, aborting\n");
			
		strcpy(RESULT, parser.m_pFullTokenizer->errorBuffer);
		strcat(RESULT, "\nerror) : Parsing failed, aborting\n");
		return RESULT;
	}

	// Generate output
	if (language == Language_GLSL)
	{
		GLSLGenerator generator;
		if (!generator.Generate(&stringLibrary, &tree, GLSLGenerator::Target(target), GLSLGenerator::Version_450, entryName))
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
		if (!generator.Generate(&stringLibrary, &tree, HLSLGenerator::Target(target), entryName, language == Language_LegacyHLSL))
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
		if (!generator.Generate(&stringLibrary, &tree, MSLGenerator::Target(target), entryName))
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

#endif

