//#define GENERATE_ORBIS

#pragma once
//#include "HLSLParser.h"

//#include "GLSLGenerator.h"
//#include "HLSLGenerator.h"
//#include "MSLGenerator.h"

#include "StringLibrary.h"

struct BindingOverride
{
	BindingOverride()
	{
		m_attrName = "";
		m_set = -1;
		m_binding = -1;
	}

	BindingOverride(const eastl::string & attrName, int set, int binding)
	{
		m_attrName = attrName;
		m_set = set;
		m_binding = binding;
	}

	eastl::string m_attrName;
	int m_set;
	int m_binding;
};

struct BindingShift
{
	char m_reg;
	int m_space;
	int m_shift;
};

class Parser
{
public:

	enum Target
	{
		Target_VertexShader,
		Target_FragmentShader,
		Target_HullShader,
		Target_DomainShader,
		Target_GeometryShader,
		Target_ComputeShader,
		Target_Num
	};

	enum Language
	{
		Language_HLSL,
		Language_GLSL,
		Language_MSL,
		Language_LegacyHLSL,
		Language_ORBIS,
		Language_SWITCH,
		Language_Num
	};


	enum Operation
	{
		Operation_Preproc, // only preprocess the file
		Operation_Parse, // preprocess and parse
		Operation_Generate, // preprocess, parse, and generate
		Operation_Num
	};

	struct Options
	{
		Options()
		{
			Reset();
		}
		
		void Reset()
		{
			mDebugTokenEnable = false;
			mDebugTokenFile = "";
			mDebugPreprocEnable = false;
			mDebugPreprocFile = "";

			mGeneratedWriteEnable = false;
			mGeneratedWriteFile = "";

			mOverrideRequired = false;

			mLanguage = (Language)-1;
			mTarget = (Target)-1;
			mOperation = (Operation)-1;

			mUseArgumentBuffers = false;
		}

		// if true, stores a tokens file
		bool mDebugTokenEnable;
		eastl::string mDebugTokenFile;

		// if true, store the output preproc file
		bool mDebugPreprocEnable;
		eastl::string mDebugPreprocFile;

		// should we actually write the generated file, or just keep it in memory
		bool mGeneratedWriteEnable;
		eastl::string mGeneratedWriteFile;

		// the language to output
		Language mLanguage;

		// the shader stage
		Target mTarget;

		bool mOverrideRequired;
		eastl::vector < BindingOverride > mOverrideVec;

		eastl::vector < BindingShift > mShiftVec;

		bool mUseArgumentBuffers;


		// what operation to actually do with the file. options are:
		// 1. Preproc
		// 2. Preproc and Parse
		// 3. Preproc, Parse, and Generate
		// In theory, we could proproc and parse once, and then generate multiple times
		// with different changes, but we'll add that option if necessary. For example,
		// compile once but generate different main() entry points or shader stages.
		Operation mOperation;
	};

	struct ParsedData
	{
		ParsedData()
		{
			Reset();
		}

		void Reset()
		{
			mLanguage = (Language)-1;
			mTarget = (Target)-1;
			mOperation = (Operation)-1;

			mIsPreprocOk = false;
			mIsParseOk = false;
			mIsGenerateOk = false;

			mIsSuccess = false;

			mDebugTokens.clear();
			mLoadedFiles.clear();

			mPreprocErrors = "";
			mParseErrors = "";
			mGenerateErrors = "";

			mPreprocData = "";

			mSrcName = "";
			mEntry = "";
			mMacroLhs.clear();
			mMacroRhs.clear();

			mGeneratedData = "";
		}

		// output language
		Language mLanguage;

		// the steps in order are:
		// 1. Preprocess
		// 2. Parse
		// 3. Generate
		bool mIsPreprocOk;
		bool mIsParseOk;
		bool mIsGenerateOk;

		// True if all operations succeeded.
		// If the operation is OPERATION_PREPROC, mIsSuccess = mIsPreprocOk.
		// If the operation is OPERATION_PARSE, mIsSuccess = mIsPreprocOk && mIsParseOk.
		// If the operation is OPERATION_GENERATE, mIsSuccess = mIsPreprocOk && mIsParseOk && mIsGenerateOk.
		// So if we don't run an operation, that operation will be listed as not ok, but mIsSuccess will be true.
		bool mIsSuccess;

		Operation mOperation;
		Target mTarget;

		eastl::string mPreprocErrors;
		eastl::string mParseErrors;
		eastl::string mGenerateErrors;

		eastl::string mPreprocData;
		eastl::vector < eastl::string > mDebugTokens;

		// a list of all the files used by the preprocessor so that we can do dependency checking
		eastl::vector < eastl::string > mLoadedFiles;

		// for convenience, the source file and preprocessor macros
		eastl::string mSrcName;
		eastl::string mEntry;
		eastl::vector < eastl::string > mMacroLhs;
		eastl::vector < eastl::string > mMacroRhs;

		eastl::string mGeneratedData;
	};

	// return value: true if succeeded, false if not
	static bool ProcessFile(
		ParsedData & parsedData,
		const eastl::string & srcFileName,
		const eastl::string & entryName,
		const Options & options,
		const eastl::vector < eastl::string > & macroLhs,
		const eastl::vector < eastl::string > & macroRhs);

	//static const char* ParserEntry(char* RESULT, const char* fileName, const char* buffer, int bufferSize, const char* entryName, const char* shader, const char* _language);
		
	static void PrintUsage();
};



