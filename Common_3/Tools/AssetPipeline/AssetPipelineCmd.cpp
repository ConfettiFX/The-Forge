#include "AssetPipeline.h"
#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../OS/Interfaces/ILogManager.h"

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
	"",    // FSR_Animtion
	"",    // FSR_OtherFiles
};

const char* gApplicationName = NULL;

void PrintHelp()
{
	printf("AssetPipelineCmd\n");
	printf("Command: processanimations \"animation/directory/\" \"output/directory/\" [flags]\n");
	printf("\t--quiet: Print only error messages.\n");
	printf("\t--force: Force all assets to be processed. Including ones that are already up-to-date.\n");
	printf("Other:\n");
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
		PrintHelp();

	tinystl::string arg = argv[1];
	arg = arg.to_lower();

	if (arg == "-h" || arg == "-help")
		PrintHelp();

	if (arg == "processanimations")
	{
		if (argc < 4)
		{
			printf("ERROR: Invalid number of arguments for command processanimations.\n");
			return 1;
		}

		tinystl::string animationDir = argv[2];
		tinystl::string outputDir = argv[3];

		bool quiet = false;
		bool force = false;
		for (int j = 4; j < argc; ++j)
		{
			arg = argv[j];
			arg = arg.to_lower();

			if (arg == "--quiet")
				quiet = true;
			else if (arg == "--force")
				force = true;
			else
				printf("WARNING: Unrecognized argument: %s\n", arg.c_str());
		}

		ProcessAssetsSettings settings = {};
		settings.quiet = quiet;
		settings.force = force;
		settings.minLastModifiedTime = appLastModified;
		if (!AssetPipeline::ProcessAnimations(animationDir.c_str(), outputDir.c_str(), &settings))
			return 1;
	}

	return 0;
};
