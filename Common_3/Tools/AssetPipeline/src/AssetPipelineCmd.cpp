/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "../../../Utilities/Interfaces/ILog.h"

#include "AssetPipeline.h"

const char* gApplicationName = "AssetPipeline";

const AssetPipelineProcessCommand gAssetPipelineCommands[] = {
    { "-pa", PROCESS_ANIMATIONS }, { "-ptfx", PROCESS_TFX },      { "-pgltf", PROCESS_GLTF },
    { "-pt", PROCESS_TEXTURES },   { "-pwz", PROCESS_WRITE_ZIP }, { "-pwza", PROCESS_WRITE_ZIP_ALL },
};
COMPILE_ASSERT(TF_ARRAY_COUNT(gAssetPipelineCommands) == PROCESS_COUNT);

void PrintHelp()
{
    printf("Asset Pipeline\n");
    printf("\n-command [flags]\n");
    printf("\nCommands:\n");
    printf("\n\t%s\t\t(GLTF to OZZ)\tProcessAnimations\n", gAssetPipelineCommands[PROCESS_ANIMATIONS].mCommandString);
    printf("\n\t%s\t(TFX to GLTF)\tProcessTFX\n", gAssetPipelineCommands[PROCESS_TFX].mCommandString);
    printf("\n\t\t--fhc | -followhaircount\t\t: Number of follow hairs around loaded guide hairs procedually\n");
    printf("\t\t--tsf | -tipseparationfactor\t: Separation factor for the follow hairs\n");
    printf("\t\t--maxradius | -maxradius\t\t: Max radius of the random distribution to generate follow hairs\n");
    printf("\n\t%s\t(GLTF to bin)\tProcessGLTF\n", gAssetPipelineCommands[PROCESS_GLTF].mCommandString);
    printf("\n\t\t--hair\t\t: Processes the mesh as a hair mesh\n");
    printf("\n\t\t--optimize\t\t: Enables all supported mesh optimization techniques\n");
    printf("\n\t\t--optimizecache\t\t: Enables vertex cache optimization\n");
    printf("\n\t\t--optimizeoverdraw\t\t: Enables overdraw optimization\n");
    printf("\n\t\t--optimizefetch\t\t: Enables vertex fetch optimization\n");
    printf("\n\t\t--meshlet\t\t: Enables meshlet generation. By default number of vertices and triangles in each meshlet is limited by 128 "
           "and 256 respectively\n");
    printf("\n\t\t--meshletnumvertices [num]\t\t: Overrides maximum number of vertices in each meshlet\n");
    printf("\n\t\t--meshletnumtriangles [num]\t\t: Overrides maximum number of triangles in each meshlet\n");
    printf("\n\t%s\t(PNG/DDS/KTX to DDS/KTX)\tProcess Textures\n", gAssetPipelineCommands[PROCESS_TEXTURES].mCommandString);
    printf("\n\t\t--astc\t\t Perform ASTC compression | default astc4x4 | overrides --astc4x4 --astc8x8 \n");
    printf("\n\t\t--bc\t\t Perform DXT BC compression | default bc3 | overrides --bc1 --bc3 --bc4 --bc5 --bc7\n");
    printf("\n\t\t--genmips\t Generate mip maps if not existing \n");
    printf("\n\t\t--in-linear\t\t Specify input Color space as Linear \n");
    printf("\n\t\t--vmf [RoughnessFileName]\t\t Create vMF filtered normal mipmaps using given roughness texture \n");
    printf("\n\t\t--normal\t\t Process texture as normal map\n");
    printf("\n\t\t--swizzle [Channels]\t\t Swizzle channels before compression. Requires input texture to be uncompressed. [Channels] is "
           "an arbitrary combination of up to 4 characters from {r,g,b,a,x,y,z,w,i,j,k,l,0,1} \n\t\t\trgba/xyzw preserve original "
           "channels, ijkl flip channels, 0 and 1 set channels to 0 or 1\n");
    printf("\n\t%s\t(filtered zip)\tProcessWriteZip\n", gAssetPipelineCommands[PROCESS_WRITE_ZIP].mCommandString);
    printf("\n\t\t--filter [extension filters]\t: Only zip files with the chosen extensions\n");
    printf("\n\t%s\t(folder to zip)\tProcessWriteZipAll\n", gAssetPipelineCommands[PROCESS_WRITE_ZIP_ALL].mCommandString);
    printf("\n\t\t--filter [subfolders to zip]\t: Only zip folders with the chosen names\n");
    printf("\nCommon Options:\n");
    printf("\n\t-h | -help\t\t: Print usage information\n");
    printf("\n\t--input [path]\t\t\t: Choose input folder\n");
    printf("\n\t--output [path]\t\t\t: Choose output folder\n");
    printf("\n\t--quiet\t\t\t: Print only error messages\n");
    printf("\n\t--force\t\t\t: Force all assets to be processed\n");
}

int AssetPipelineCmd(int argc, char** argv)
{
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

    if (argc < 2)
    {
        printf("Invalid command.\n");
        return 0;
    }

    if (!initMemAlloc(gApplicationName))
        return EXIT_FAILURE;

    const char* input = "";
    const char* output = "";

    // Parse commands, fill params
    AssetPipelineParams params = {};
    params.mSettings.force = false;
    params.mSettings.quiet = false;
    params.mInFilePath = input;
    params.mInExt = "";
    params.mFlagsCount = 0;
    params.mPathMode = PROCESS_MODE_NONE;

    params.mRDInput = RD_MIDDLEWARE_1;
    params.mRDOutput = RD_MIDDLEWARE_2;
    params.mRDZipWrite = RD_MIDDLEWARE_3;

    char filePath[FS_MAX_PATH] = { 0 };
    char fileNameWithoutExt[FS_MAX_PATH] = { 0 };
    char fileName[FS_MAX_PATH] = { 0 };
    char iext[FS_MAX_PATH] = { 0 };

    for (int i = 2; i < argc; ++i)
    {
        const char* arg = argv[i];

        if (STRCMP(arg, "--input"))
        {
            if (params.mPathMode == PROCESS_MODE_NONE)
            {
                params.mPathMode = PROCESS_MODE_DIRECTORY;
            }
            input = argv[++i];
        }
        else if (STRCMP(arg, "--input-file"))
        {
            if (params.mPathMode == PROCESS_MODE_DIRECTORY_RECURSIVE)
            {
                LOGF(eERROR, "--recursive flag not allowed on single file mode");
                return 1;
            }

            i++;
            params.mPathMode = PROCESS_MODE_FILE;
            fsGetPathFileName(argv[i], fileNameWithoutExt);
            fsGetPathExtension(argv[i], iext);
            params.mInExt = iext;
            fsAppendPathExtension(fileNameWithoutExt, iext, fileName);
            params.mInFilePath = fileName;

            fsGetParentPath(argv[i], filePath);
            input = filePath;
        }
        else if (STRCMP(arg, "--recursive"))
        {
            if (params.mPathMode == PROCESS_MODE_FILE)
            {
                LOGF(eERROR, "--recursive flag not allowed on single file mode");
                return 1;
            }

            params.mPathMode = PROCESS_MODE_DIRECTORY_RECURSIVE;
        }
        else if (STRCMP(arg, "--output"))
        {
            output = argv[++i];
        }
        else if (STRCMP(arg, "--quiet"))
        {
            params.mSettings.quiet = true;
        }
        else if (STRCMP(arg, "--force"))
        {
            params.mSettings.force = true;
        }
        else
        {
            params.mFlags[params.mFlagsCount++] = argv[i];
        }
    }

    params.mInDir = input;
    params.mOutDir = output;

    FileSystemInitDesc fsDesc = {};
    fsDesc.pAppName = gApplicationName;
    fsDesc.mIsTool = true;

    if (!initFileSystem(&fsDesc))
    {
        LOGF(eERROR, "Filesystem failed to initialize.");
        exitMemAlloc();
        return 1;
    }

    char   inputPath[FS_MAX_PATH] = { 0 };
    size_t size = fsNormalizePath(input, '/', inputPath);
    if (inputPath[size - 1] != '/')
        inputPath[size] = '/';

    char outputPath[FS_MAX_PATH] = { 0 };
    fsNormalizePath(output, '/', outputPath);
    if (outputPath[size - 1] != '/')
        outputPath[size] = '/';

    fsSetPathForResourceDir(pSystemFileIO, params.mRDInput, inputPath);
    fsSetPathForResourceDir(pSystemFileIO, params.mRDOutput, outputPath);
    fsSetPathForResourceDir(pSystemFileIO, RD_LOG, "");

    LogLevel logLevel = params.mSettings.quiet ? eWARNING : DEFAULT_LOG_LEVEL;
    initLog(gApplicationName, logLevel);

    bool runAssetPipeline = false;
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(gAssetPipelineCommands); ++i)
    {
        if (STRCMP(argv[1], gAssetPipelineCommands[i].mCommandString))
        {
            params.mProcessType = gAssetPipelineCommands[i].mProcessType;
            runAssetPipeline = true;
            break;
        }
    }

    // ...

    int ret = 0;

    if (runAssetPipeline)
    {
        // Make sure output folder exists before starting the pipeline
        if (fsCreateDirectory(params.mRDOutput, "", true))
        {
            ret = AssetPipelineRun(&params);
        }
    }
    else
    {
        LOGF(eERROR, "Unrecognized argument %s.", argv[1]);
    }

    exitLog();
    exitFileSystem();
    exitMemAlloc();

    return ret;
}

int main(int argc, char** argv) { return AssetPipelineCmd(argc, argv); }
