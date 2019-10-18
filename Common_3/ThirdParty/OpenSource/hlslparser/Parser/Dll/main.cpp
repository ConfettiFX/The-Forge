#include "..\Parser\Engine.h"
#include "..\Parser\Parser.h"
#include <Objbase.h>

extern "C" __declspec(dllexport) char* PARSER(const char* fileName, const char* entryName, const char* shader, char* _language)
{
	Parser::Target target = Parser::Target_Num;
	Parser::Language language = Parser::Language_Num;

	eastl::string StageName;
 
	bool error = false;

	if (String_Equal(shader, "-fs"))
	{
		target = Parser::Target_FragmentShader;
		StageName.append("frag");
	}
	else if (String_Equal(shader, "-vs"))
	{
		target = Parser::Target_VertexShader;
		StageName.append("vert");
	}
	else if (String_Equal(shader, "-hs"))
	{
		target = Parser::Target_HullShader;
		StageName.append("tesc");
	}
	else if (String_Equal(shader, "-ds"))
	{
		target = Parser::Target_DomainShader;
		StageName.append("tese");
	}
	else if (String_Equal(shader, "-gs"))
	{
		target = Parser::Target_GeometryShader;
		StageName.append("geom");
	}
	else if (String_Equal(shader, "-cs"))
	{
		target = Parser::Target_ComputeShader;
		StageName.append("comp");
	}

	if (String_Equal(_language, "-glsl"))
	{
		language = Parser::Language_GLSL;
	}
	else if (String_Equal(_language, "-hlsl"))
	{
		language = Parser::Language_HLSL;
	}
	else if (String_Equal(_language, "-legacyhlsl"))
	{
		// not really supported
		language = Parser::Language_HLSL;
	}
	else if (String_Equal(_language, "-msl"))
	{
		language = Parser::Language_MSL;
	}
#ifdef GENERATE_ORBIS
	else if (String_Equal(_language, "-orbis"))
	{
		language = Parser::Language_ORBIS;
	}
#endif

	Parser::Options options;
	Parser::ParsedData parsedData;

	eastl::string dstPreprocName = "";//FileName + eastl::string("_") + StageName + eastl::string("_preproc.txt");
	eastl::string dstTokenName = "";// dstDir + baseName + "_" + stage + "_token.txt";

	options.mDebugPreprocEnable = false;
	options.mDebugPreprocFile = dstPreprocName;
	options.mDebugTokenEnable = false;
	options.mDebugTokenFile = dstTokenName;
	options.mGeneratedWriteEnable = false;
	options.mLanguage = language;
	options.mOperation = Parser::Operation_Generate;
	options.mTarget = target;

	eastl::vector < eastl::string > macroLhs{"UPDATE_FREQ_NONE", "UPDATE_FREQ_PER_FRAME", "UPDATE_FREQ_PER_BATCH", "UPDATE_FREQ_PER_DRAW"};
	eastl::vector < eastl::string > macroRhs{"space0", "space1", "space2", "space3"};

	Parser::ProcessFile(parsedData, fileName, entryName, options, macroLhs, macroRhs);

	unsigned long ulSize = (unsigned long)parsedData.mGeneratedData.size() + (unsigned long)sizeof(char);
	char* pszReturn = NULL;
	pszReturn = (char*)::CoTaskMemAlloc(ulSize);
	strcpy_s(pszReturn, ulSize, parsedData.mGeneratedData.c_str());
	return  pszReturn;
}
