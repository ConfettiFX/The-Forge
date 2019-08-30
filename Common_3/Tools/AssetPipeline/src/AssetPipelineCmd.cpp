#include "AssetPipeline.h"
#include "../../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../../OS/Interfaces/ILog.h"

#include <cstdio>
#include <sys/stat.h>

const char* pszBases[] = {
	"",    // FSR_BinShaders
	"",    // FSR_SrcShaders
	"",    // FSR_BinShaders_Common
	"",    // FSR_SrcShaders_Common
	"",    // FSR_Textures
	"",    // FSR_Meshes
	"",    // FSR_Builtin_Fonts
	"",    // FSR_GpuConfig
	"",    // FSR_Animation
	"",    // FSR_Audio
	"",    // FSR_OtherFiles
};

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

size_t GetFileLastModifiedTime(const char* _fileName)
{
	struct stat fileInfo;

	if (!stat(_fileName, &fileInfo))
	{
		return (size_t)fileInfo.st_mtime;
	}
	else
	{
		// return an impossible large mod time as the file doesn't exist
		return ~0;
	}
}

int main(int argc, char** argv)
{
	uint appLastModified = 0;
	if (argc > 0)
	{
		gApplicationName = argv[0];
		appLastModified = (uint)GetFileLastModifiedTime(gApplicationName);
	}

	if (argc == 1)
	{
		PrintHelp();
		return 0;
	}

	eastl::string arg = argv[1];
	eastl::string command = arg;
	arg.make_lower();

	if (arg == "-h" || arg == "-help")
	{
		PrintHelp();
		return 0;
	}

	if (argc < 4)
	{
		printf("ERROR: Invalid number of arguments for command processanimations.\n");
		return 1;
	}

	eastl::string inputDir = argv[2];
	eastl::string outputDir = argv[3];

	ProcessAssetsSettings settings = {};
	settings.quiet = false;
	settings.force = false;
	settings.minLastModifiedTime = appLastModified;
	settings.quantizePositionBits = 16;
	settings.quantizeTexBits = 16;
	settings.quantizeNormalBits = 8;

	for (int i = 4; i < argc; ++i)
	{
		arg = argv[i];
		arg.make_lower();

		if (arg == "--quiet")
		{
			settings.quiet = true;
		}
		else if (arg == "--force")
		{
			settings.force = true;
		}
		if (arg == "-posbits")
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.quantizePositionBits = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg.c_str());

			if (settings.quantizePositionBits < 1 || settings.quantizePositionBits > 16)
			{
				printf("WARNING: Argument outide of range 1-16: %s\n", arg.c_str());
				printf("         Using default value\n");
				settings.quantizePositionBits = 16;
			}
		}
		else if (arg == "-texbits")
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.quantizeTexBits = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg.c_str());

			if (settings.quantizeTexBits < 1 || settings.quantizeTexBits > 16)
			{
				printf("WARNING: Argument outide of range 1-16: %s\n", arg.c_str());
				printf("         Using default value\n");
				settings.quantizeTexBits = 16;
			}
		}
		else if (arg == "-normbits")
		{
			if (i + 1 < argc && isdigit(argv[i + 1][0]))
				settings.quantizeNormalBits = atoi(argv[++i]);
			else
				printf("WARNING: Argument expects a value: %s\n", arg.c_str());


			if (settings.quantizeNormalBits < 1 || settings.quantizeNormalBits > 8)
			{
				printf("WARNING: Argument outide of range 1-8: %s\n", arg.c_str());
				printf("         Using default value\n");
				settings.quantizeNormalBits = 8;
			}
		}
		else
		{
			printf("WARNING: Unrecognized argument: %s\n", arg.c_str());
		}
	}

	if (command == "processanimations")
	{
		if (!AssetPipeline::ProcessAnimations(inputDir.c_str(), outputDir.c_str(), &settings))
			return 1;
	}
	else if (command == "processmeshes")
	{
		if (!AssetPipeline::ProcessModels(inputDir.c_str(), outputDir.c_str(), &settings))
			return 1;
	}
	else if (command == "processtextures")
	{
		if (!AssetPipeline::ProcessTextures(inputDir.c_str(), outputDir.c_str(), &settings))
			return 1;
	}
	else
	{
		printf("ERROR: Invalid command.\n");
	}
	return 0;
};
