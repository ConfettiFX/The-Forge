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

#include "../../../../../OS/Interfaces/IFileSystem.h"


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

int ParserMain( int argc, char* argv[] )
{
	//using namespace M4;
#ifdef TEST_PARSER
	return ParserTest();
#else
	// Parse arguments
	const char* entryName = NULL;
	const char* outputFile = NULL;
	const char* srcFile = NULL;

	//for hull shader in Metal
	eastl::string source;

	Parser::Target target = Parser::Target_Num;
	Parser::Language language = Parser::Language_Num;

	eastl::string StageName;
 
	bool error = false;

	eastl::vector < BindingShift > bindingShift;

	bool useArgumentBuffers = false;

	int i = 1;
	while (i < argc)
	{
		if (String_Equal(argv[i], "-fs"))
		{
			target = Parser::Target_FragmentShader;
			StageName.append("frag");
		}
		else if (String_Equal(argv[i], "-vs"))
		{
			target = Parser::Target_VertexShader;
			StageName.append("vert");
		}
		else if (String_Equal(argv[i], "-hs"))
		{
			target = Parser::Target_HullShader;
			StageName.append("tesc");
		}
		else if (String_Equal(argv[i], "-ds"))
		{
			target = Parser::Target_DomainShader;
			StageName.append("tese");
		}
		else if (String_Equal(argv[i], "-gs"))
		{
			target = Parser::Target_GeometryShader;
			StageName.append("geom");
		}
		else if (String_Equal(argv[i], "-cs"))
		{
			target = Parser::Target_ComputeShader;
			StageName.append("comp");
		}
		else if (String_Equal(argv[i], "-glsl"))
		{
			language = Parser::Language_GLSL;
		}
		else if (String_Equal(argv[i], "-hlsl"))
		{
			language = Parser::Language_HLSL;
		}
		else if (String_Equal(argv[i], "-legacyhlsl"))
		{
			// not really supported
			language = Parser::Language_HLSL;
		}
		else if (String_Equal(argv[i], "-msl"))
		{
			language = Parser::Language_MSL;
		}
#ifdef GENERATE_ORBIS
		else if (String_Equal(argv[i], "-orbis"))
		{
			language = Parser::Language_ORBIS;
		}
#endif
		else if (String_Equal(argv[i], "-Fo"))
		{
			if (i+1 < argc)
			{
				outputFile = argv[++i];
			}
			else
			{
				break;
			}
		}
		else if (String_Equal(argv[i], "-E"))
		{
			if (i+1 < argc)
			{
				entryName = argv[++i];
			}
			else
			{
				break;
			}
		}
		else if (String_Equal(argv[i], "-fvk-b-shift"))
		{
			if (i+2 < argc)
			{
				int shift = atoi(argv[++i]);
				int space = atoi(argv[++i]);
				bindingShift.push_back(BindingShift{'b', space, shift});
			}
			else
			{
				break;
			}
		}
		else if (String_Equal(argv[i], "-fvk-s-shift"))
		{
			if (i+2 < argc)
			{
				int shift = atoi(argv[++i]);
				int space = atoi(argv[++i]);
				bindingShift.push_back(BindingShift{'s', space, shift});
			}
			else
			{
				break;
			}
		}
		else if (String_Equal(argv[i], "-fvk-t-shift"))
		{
			if (i+2 < argc)
			{
				int shift = atoi(argv[++i]);
				int space = atoi(argv[++i]);
				bindingShift.push_back(BindingShift{'t', space, shift});
			}
			else
			{
				break;
			}
		}
		else if (String_Equal(argv[i], "-fvk-u-shift"))
		{
			if (i+2 < argc)
			{
				int shift = atoi(argv[++i]);
				int space = atoi(argv[++i]);
				bindingShift.push_back(BindingShift{'u', space, shift});
			}
			else
			{
				break;
			}
		}
		else if (String_Equal(argv[i], "-useargbuffers"))
		{
			useArgumentBuffers = true;
		}
		else
		{
			if (!srcFile) srcFile = argv[i];
			source = source + ReadFile(argv[i]);
		}
		++i;
	}

	Parser::Options options;
	Parser::ParsedData parsedData;

	if (source.empty())
	{
		printf("Nothing to parse\n");
		return 1;
	}

	if (language>=Parser::Language_Num || StageName.empty() || !outputFile || !entryName)
	{
		printf("Invalid command line");
		return 1;
	}

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
	options.mShiftVec = bindingShift;
	options.mUseArgumentBuffers = useArgumentBuffers;

	eastl::vector < eastl::string > macroLhs{"UPDATE_FREQ_NONE", "UPDATE_FREQ_PER_FRAME", "UPDATE_FREQ_PER_BATCH", "UPDATE_FREQ_PER_DRAW"};
	eastl::vector < eastl::string > macroRhs{"space0", "space1", "space2", "space3"};

	return Parser::ProcessFile(parsedData, srcFile, entryName, options, macroLhs, macroRhs) ? 0 : 1;
#endif

	return 0;
}

int main(int argc, char** argv)
{
	extern bool MemAllocInit(const char*);
	extern void MemAllocExit();

	if (!MemAllocInit("HLSLParser"))
		return EXIT_FAILURE;

	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = "HLSLParser";
	if (!initFileSystem(&fsDesc))
		return EXIT_FAILURE;

	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

	int ret = ParserMain(argc, argv);

	exitFileSystem();
	MemAllocExit();

	return ret;
}
