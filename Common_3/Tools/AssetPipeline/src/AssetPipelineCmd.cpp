#include "AssetPipeline.h"
#include "../../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../../OS/Interfaces/ILog.h"

#include <cstdio>
#include <sys/stat.h>

#include "../../../OS/Interfaces/IMemory.h"

const char* gApplicationName = NULL;

void PrintHelp()
{
	printf("AssetPipelineCmd\n");
	printf("\nCommand: processanimations \"animation/directory/\" \"output/directory/\" [flags]\n");
	printf("\t--quiet: Print only error messages.\n");
	printf("\t--force: Force all assets to be processed. Including ones that are already up-to-date.\n");
	printf("\nCommand: processmeshes \"meshes/directory/\" \"output/directory/\" [arguments]\n");
	printf("\t-posbits N: use N-bit quantization for positions (default: 16; N should be between 1 and 16)\n");
	printf("\t-texbits N: use N-bit quantization for texture coordinates (default: 12; N should be between 1 and 16)\n");
	printf("\t-normbits N: use N-bit quantization for normals and tangents (default: 8; N should be between 1 and 8)\n");
	printf("\nCommand: processtextures \"textures/directory/\" \"output/directory/\" \n");
	printf("\nOther:\n");
	printf("\t-h or -help: Print usage information.\n");
}

int AssetPipelineCmd(int argc, char** argv)
{
	time_t appLastModified = 0;
	if (argc > 0)
	{
		gApplicationName = argv[0];
        PathHandle applicationPath = fsCopyExecutablePath();
		appLastModified = fsGetLastModifiedTime(applicationPath);
	}

	if (argc == 1)
	{
		PrintHelp();
		return 0;
	}

	if (stricmp(argv[1], "-h") == 0 || stricmp(argv[1], "-help") == 0)
	{
		PrintHelp();
		return 0;
	}

	if (argc < 4)
	{
		printf("ERROR: Invalid number of arguments for command processanimations.\n");
		return 1;
	}

    FileSystem* fileSystem = fsGetSystemFileSystem();
    PathHandle workingDir = fsCopyWorkingDirectoryPath();
    
	PathHandle inputDir = fsCreatePath(fileSystem, argv[2]);
    if (!inputDir)
        inputDir = fsAppendPathComponent(workingDir, argv[2]);
    
	PathHandle outputDir = fsCreatePath(fileSystem, argv[3]);
    if (!outputDir)
        outputDir = fsAppendPathComponent(workingDir, argv[3]);

	ProcessAssetsSettings settings = {};
	settings.quiet = false;
	settings.force = false;
	settings.minLastModifiedTime = (unsigned int)appLastModified;
	settings.quantizePositionBits = 16;
	settings.quantizeTexBits = 16;
	settings.quantizeNormalBits = 8;

	const char* command = argv[1];

	for (int i = 4; i < argc; ++i)
	{
		const char* arg = argv[i];

		if (stricmp(arg, "--quiet") == 0)
		{
			settings.quiet = true;
		}
		else if (stricmp(arg, "--force") == 0)
		{
			settings.force = true;
		}
		if (stricmp(arg, "-posbits") == 0)
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.quantizePositionBits = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg);

			if (settings.quantizePositionBits < 1 || settings.quantizePositionBits > 16)
			{
				printf("WARNING: Argument outide of range 1-16: %s\n", arg);
				printf("         Using default value\n");
				settings.quantizePositionBits = 16;
			}
		}
		else if (stricmp(arg, "-texbits") == 0)
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.quantizeTexBits = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg);

			if (settings.quantizeTexBits < 1 || settings.quantizeTexBits > 16)
			{
				printf("WARNING: Argument outide of range 1-16: %s\n", arg);
				printf("         Using default value\n");
				settings.quantizeTexBits = 16;
			}
		}
		else if (stricmp(arg, "-normbits") == 0)
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.quantizeNormalBits = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg);


			if (settings.quantizeNormalBits < 1 || settings.quantizeNormalBits > 8)
			{
				printf("WARNING: Argument outide of range 1-8: %s\n", arg);
				printf("         Using default value\n");
				settings.quantizeNormalBits = 8;
			}
		}
		else if (stricmp(arg, "-followhaircount") == 0 || stricmp(arg, "--fhc") == 0)
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.mFollowHairCount = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg);
		}
		else if (stricmp(arg, "-tipseparationfactor") == 0 || stricmp(arg, "--tsf") == 0)
		{
			settings.mTipSeperationFactor = (float)atof(argv[++i]);
		}
		else if (stricmp(arg, "-maxradius") == 0 || stricmp(arg, "--maxradius") == 0)
		{
			settings.mMaxRadiusAroundGuideHair = (float)atof(argv[++i]);
		}
		else
		{
			printf("WARNING: Unrecognized argument: %s\n", arg);
		}
	}

	if (stricmp(command, "-pa") == 0)
	{
		if (!AssetPipeline::ProcessAnimations(inputDir, outputDir, &settings))
			return 1;
	}
	else if (stricmp(command, "-pt") == 0)
	{
		if (!AssetPipeline::ProcessTextures(inputDir, outputDir, &settings))
			return 1;
	}
	else if (stricmp(command, "-pvt") == 0)
	{
		if (!AssetPipeline::ProcessVirtualTextures(inputDir, outputDir, &settings))
			return 1;
	}
	else if (stricmp(command, "-ptfx") == 0)
	{
		if (!AssetPipeline::ProcessTFX(inputDir, outputDir, &settings))
			return 1;
	}
	else
	{
		printf("ERROR: Invalid command. %s\n", command);
	}

	return 0;
}

int main(int argc, char** argv)
{
	extern bool MemAllocInit();
	extern void MemAllocExit();

	if (!MemAllocInit())
		return EXIT_FAILURE;

	if (!fsInitAPI())
		return EXIT_FAILURE;

	Log::Init();

	int ret = AssetPipelineCmd(argc, argv);

	Log::Exit();
	fsDeinitAPI();
	MemAllocExit();

	return ret;
}
