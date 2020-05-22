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
	printf(
		"\nCommand: ProcessAnimations          (GLTF to OZZ) -pa   \"animation/directory/\" \"output/directory/\" [flags]\n"
		"\nCommand: ProcessVirtualTextures     (DDS to SVT)  -pvt  \"source texture directory/\" \"output directory/\" [flags]\n"
		"\nCommand: ProcessTFX                 (TFX to GLTF) -ptfx \"source tfx directory/\" \"output directory/\" [flags]\n"
			"\t --fhc | -followhaircount      : Number of follow hairs around loaded guide hairs procedually\n"
			"\t --tsf | -tipseparationfactor  : Separation factor for the follow hairs\n"
			"\t --maxradius | -maxradius      : Max radius of the random distribution to generate follow hairs\n"
		"\nCommon Options:\n"
			"\t --quiet                       : Print only error messages.\n"
			"\t --force                       : Force all assets to be processed. Including ones that are already up-to-date.\n"
			"\t -h | -help                    : Print usage information.\n");
}

int AssetPipelineCmd(int argc, char** argv)
{
	time_t appLastModified = 0;
	if (argc > 0)
	{
		gApplicationName = argv[0];
        PathHandle applicationPath = fsGetApplicationPath();
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
    PathHandle workingDir = fsGetApplicationDirectory();
    
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
	fsExitAPI();
	MemAllocExit();

	return ret;
}
