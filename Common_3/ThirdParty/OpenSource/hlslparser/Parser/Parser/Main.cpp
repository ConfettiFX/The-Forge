#include "Parser.h"
#include "Engine.h"

#include <fstream>
#include <sstream>
#include <direct.h> // for _mkdir


#include <string>
#include <vector>

#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"

#define NOMINMAX
#include <Windows.h>


eastl::string ReadFile(const char* fileName)
{
	std::ifstream ifs(fileName);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return eastl::string(buffer.str().c_str());
}

bool WriteFile(const char* fileName, const char* contents)
{
	//check first if there is right folder along the path of filename
	//And make directory	

	size_t found;
	eastl::string FilenameStr(fileName);
	found = FilenameStr.find_last_of("/\\");
	eastl::string DirnameStr = FilenameStr.substr(0, found);

	_mkdir(DirnameStr.c_str());

	std::ofstream ofs(fileName);

	ofs << contents;
	ofs.close();
	return true;
}

#if 0
const char* getIncludeFiles(char newDirPath[], const char* pIncludedFileName[MAX_INCLUDE_FILE+1], int *pIncludedCounter, const char* pIncluded[MAX_INCLUDE_FILE], std::string originFile)
{	
	char toCstr[65536];
	strcpy(toCstr, originFile.c_str());

	size_t index = 0;

	while (true)
	{
		index = originFile.find("#include", index);

		if (std::string::npos == index)
		{
			break;
		}

		strcpy(toCstr, originFile.substr(index).c_str());

		//char* includeFile = std::strtok(toCstr, " \n\t");
		char* includeFile = std::strtok(toCstr, " \"<\n\t");
		includeFile = std::strtok(NULL, " \"<\n\t");


		char* newTempFilePath = new char[256];
		strcpy(newTempFilePath, newDirPath);
		strcat(newTempFilePath, includeFile);

		std::string newContents = ReadFile(newTempFilePath);

		if (newContents == "")
		{
			//can't find include files
			return includeFile;
		}


		char newDirPath[256];
		char drive[16];
		char dir[256];

		_splitpath_s(newTempFilePath,
			drive, sizeof(drive),
			dir, sizeof(dir),
			NULL, 0,             // Don't need filename
			NULL, 0);


		strcpy(newDirPath, drive);
		strcat(newDirPath, dir);
		

		//check first it also has Include files
		const char* RESULT = getIncludeFiles(newDirPath, pIncludedFileName, pIncludedCounter, pIncluded, newContents);
		if (RESULT)
		{
			//can't find include files
			return RESULT;
		}


		char* temp = new char[65536];

		strcpy(temp, newContents.data());


		pIncludedFileName[(*pIncludedCounter)] = newTempFilePath;
		pIncluded[(*pIncludedCounter)++] = temp;

		index += 8;
	}

	return NULL;
}


void removeIncludedFileNames(int includedCounter, const char* pIncludedFileName[MAX_INCLUDE_FILE + 1])
{
	// don't forget to delete includefile contents
	for (int i = 0; i < includedCounter + 1; i++)
	{
		delete[] pIncludedFileName[i];
	}
}

void removeIncludedFiles(int includedCounter, const char* pIncluded[MAX_INCLUDE_FILE])
{
	// don't forget to delete includefile contents
	for (int i = 0; i < includedCounter; i++)
	{
		delete[] pIncluded[i];
	}
}
#endif

extern int ParserTest();

int main( int argc, char* argv[] )
{
	//using namespace M4;
#ifdef TEST_PARSER
	return ParserTest();
#else
	// Parse arguments
	const char* fileName = NULL;
	const char* entryName = NULL;
	//const char* shader = NULL;
	//const char* _language = NULL;
	const char* outputFile = NULL;

	//for hull shader in Metal
	const char* secondaryfileName = NULL;
	const char* secondaryentryName = NULL;

	Parser::Target target = Parser::Target_VertexShader;
	Parser::Language language = Parser::Language_GLSL;

  eastl::string StageName;
 

	if (String_Equal(argv[1], "-fs"))
	{
		target = Parser::Target_FragmentShader;
    StageName.append("frag");
	}
	else if (String_Equal(argv[1], "-vs"))
	{
		target = Parser::Target_VertexShader;
    StageName.append("vert");
	}
	else if (String_Equal(argv[1], "-hs"))
	{
		target = Parser::Target_HullShader;
    StageName.append("tesc");
	}
	else if (String_Equal(argv[1], "-ds"))
	{
		target = Parser::Target_DomainShader;
    StageName.append("tese");
	}
	else if (String_Equal(argv[1], "-gs"))
	{
		target = Parser::Target_GeometryShader;
    StageName.append("geom");
	}
	else if (String_Equal(argv[1], "-cs"))
	{
		target = Parser::Target_ComputeShader;
    StageName.append("comp");
	}

	if (String_Equal(argv[2], "-glsl"))
	{
		language = Parser::Language_GLSL;
	}
	else if (String_Equal(argv[2], "-hlsl"))
	{
		language = Parser::Language_HLSL;
	}
	else if (String_Equal(argv[2], "-legacyhlsl"))
	{
		// not really supported
		language = Parser::Language_HLSL;
	}
	else if (String_Equal(argv[2], "-msl"))
	{
		language = Parser::Language_MSL;
	}


	if (fileName == NULL)
	{
		fileName = argv[3];
	}

	if (entryName == NULL)
	{
		entryName = argv[4];
	}
	
	if (argc >= 5 && outputFile == NULL)
	{
		outputFile = argv[5];
	}

	if (argc >= 6 && secondaryfileName == NULL)
	{
		secondaryfileName = argv[6];
	}

	if (argc >= 7 && secondaryentryName == NULL)
	{
		secondaryentryName = argv[7];
	}


  eastl::string srcFileName(fileName);

	// Read input file
	eastl::string source = ReadFile(fileName);
	
	if (secondaryfileName)
	{
		eastl::string source2 = ReadFile(secondaryfileName);
		source = source2 + (source);
	}
	

	Parser::Options options;
	Parser::ParsedData parsedData;

	//options.mDebugPreprocFile = 
	eastl::string FileName(fileName);

	eastl::string dstPreprocName = "";//FileName + eastl::string("_") + StageName + eastl::string("_preproc.txt");
	eastl::string dstTokenName = "";// dstDir + baseName + "_" + stage + "_token.txt";
	eastl::string dstGeneratedName = outputFile;// dstDir + baseName + "." + stage;

	options.mDebugPreprocEnable = false;
	options.mDebugPreprocFile = dstPreprocName;
	options.mDebugTokenEnable = false;
	options.mDebugTokenFile = dstTokenName;
	options.mGeneratedWriteEnable = true;
	options.mGeneratedWriteFile = dstGeneratedName;
	options.mLanguage = language;
	options.mOperation = Parser::Operation_Generate;
	options.mTarget = target;


	eastl::vector < eastl::string > macroLhs;
	eastl::vector < eastl::string > macroRhs;

	Parser::ProcessFile(parsedData, srcFileName, entryName, options, macroLhs, macroRhs);
#endif
}
