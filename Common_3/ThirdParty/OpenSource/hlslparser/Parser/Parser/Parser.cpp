#include "Parser.h"

#include "HLSLParser.h"
#include "HLSLGenerator.h"
#include "HLSLTokenizer.h"
#include "MSLGenerator.h"
#include "GLSLGenerator.h"
#include "MCPPPreproc.h"

#include <direct.h>

#pragma warning(disable:4996)

#ifdef GENERATE_ORBIS
#include "../../../../../../PS4/Common_3/ThirdParty/OpenSource/hlslparser/OrbisGenerator.cpp"
#endif

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

	if (found != std::string::npos)
	{
		_mkdir(DirnameStr.c_str());
	}

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
		GLSLGenerator::Options glslOptions;
		glslOptions.shiftVec = options.mShiftVec;
		GLSLGenerator generator;
		parsedData.mIsGenerateOk = generator.Generate(&stringLibrary, &tree, (GLSLGenerator::Target)options.mTarget, GLSLGenerator::Version_450, entryName.c_str(), glslOptions);
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
		mslOptions.shiftVec = options.mShiftVec;
		mslOptions.useArgBufs = options.mUseArgumentBuffers;

		parsedData.mIsGenerateOk = generator.Generate(&stringLibrary, &tree, (MSLGenerator::Target)options.mTarget, entryName.c_str(), mslOptions);
		parsedData.mGeneratedData = generator.GetResult();
	}
#ifdef GENERATE_ORBIS
	else if (options.mLanguage == Language_ORBIS)
	{
		OrbisGenerator generator;
		parsedData.mIsGenerateOk = generator.Generate(&stringLibrary, &tree, (OrbisGenerator::Target)options.mTarget, entryName.c_str());
		parsedData.mGeneratedData = generator.GetResult();
	}
#endif
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
