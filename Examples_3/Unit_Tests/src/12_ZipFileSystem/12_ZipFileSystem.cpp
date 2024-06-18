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

// Unit Test for testing transformations using a solar system.
// Tests the basic mat4 transformations, such as scaling, rotation, and translation.

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"
// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define SIZEOF_ARR(x) sizeof(x) / sizeof(x[0])

/// Demo structures

struct UniformBlock
{
    CameraMatrix mProjectView;
    mat4         mModelMatCapsule;
    mat4         mModelMatCube;
    mat4         mModelOcclusion;
};

struct Vertex
{
    float3 mPosition;
    float3 mNormal;
    float2 mUV;
};

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

ProfileToken gGpuProfileToken;
Renderer*    pRenderer = NULL;

const int   gSphereResolution = 128;
const float gSphereDiameter = 0.5f;

int    gNumberOfSpherePoints = 0;
float* pSpherePoints = nullptr;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Semaphore*    pImageAcquiredSemaphore = NULL;

Shader*   pBasicShader = NULL;
Pipeline* pBasicPipeline = NULL;

Shader*        pSkyboxShader = NULL;
Buffer*        pSkyboxVertexBuffer = NULL;
Pipeline*      pPipelineSkybox = NULL;
RootSignature* pRootSignature = NULL;
Sampler*       pSamplerSkybox = NULL;
Texture*       pSkyboxTextures[6];

// Zip File Test Texture
Texture*  pZipTexture[1];
Shader*   pZipTextureShader = NULL;
Shader*   pOcclusionShader = NULL;
Buffer*   pZipTextureVertexBuffer = NULL;
Pipeline* pZipTexturePipeline = NULL;

// Occlusion Query
const uint32_t gMaxOcclusionQueries = 2;
const uint32_t gOccTestOccuionSphereMaxIndex = 0;
const uint32_t gOccTestOccuionSphereIndex = 1;

QueryPool* pOcclusionQueryPool[gDataBufferCount] = {};
Pipeline*  pOcclusionMax = NULL;
Pipeline*  pOcclusionTest = NULL;
Buffer*    pSphereVertexBuffer = NULL;

unsigned char gOcclsuionText[256];
bstring       gOcclusionbstr = bemptyfromarr(gOcclsuionText);
float4        gOccluion1Color = float4(0.0f, 1.0f, 0.0f, 1.0f);

Buffer* pProjViewUniformBuffer[gDataBufferCount] = { NULL };

DescriptorSet* pDescriptorSetFrameUniforms = NULL;
DescriptorSet* pDescriptorSetTextures = NULL;

uint32_t gFrameIndex = 0;

int          gNumberOfBasicPoints;
int          gNumberOfCubiodPoints;
UniformBlock gUniformData;

ICameraController* pCameraController = NULL;

const char* pSkyboxImageFileNames[] = {
    "Skybox/Skybox_right1.tex",  "Skybox/Skybox_left2.tex",  "Skybox/Skybox_top3.tex",
    "Skybox/Skybox_bottom4.tex", "Skybox/Skybox_front5.tex", "Skybox/Skybox_back6.tex",
};

const char* pCubeTextureName[] = { "TheForge.tex" };

const char* pTextFileName[] = { "TestDoc.txt" };

const ResourceDirectory RD_ARCHIVE_TEST = RD_MIDDLEWARE_1;

const char* pModelFileName[] = { "matBall.bin" };

FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;

UIComponent* pGui_TextData = NULL;
UIComponent* pGui_OcclusionData = NULL;
UIComponent* pGui_ZipData = NULL;

// Zip file for testing
const char* pZipReadFile = "28-ArchiveFileSystem.buny";

bstring gText = bempty();

// structures for loaded model
Geometry*    pMesh;
VertexLayout gVertexLayoutDefault = {};

const char* pMTunerOut = "testout.txt";
IFileSystem gArchiveFileSystem = { 0 };

// numbers stored in a file they way each 8 byte value is an index (0,1,2,3,4,5,...).
// This way we can easily test seek feature for various compression formats.
const int   gNumbersFileCount = 3;
const char* gNumbersFileNames[] = {
    "numbers.raw",
    "numbers.lz4",
    "numbers.zstd",
};

#if 0 // numbers files generator source code
int main()
{
	FILE* f = fopen("numbers", "wb");
	for (uint64_t i = 0; i < 512 * 1024; ++i)
		fwrite(&i, 8, 1, f);
	fclose(f);
}
#endif

typedef bool (*findStreamCb)(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition);

static bool testFindStream(const char* name, findStreamCb findCb)
{
    char       txt[] = "THIS IS A TEST TEXT";
    const char pattern[] = "TEST";
    FileStream stream;
    fsOpenStreamFromMemory(txt, sizeof(txt), FM_READ, false, &stream);
    if (findCb == fsFindReverseStream)
        fsSeekStream(&stream, SBO_END_OF_FILE, 0);
    ssize_t pos = INT_MAX;
    findCb(&stream, pattern, sizeof(pattern) - 1, INT_MAX, &pos);
    fsCloseStream(&stream);
    if (pos != 10)
    {
        LOGF(eERROR, "Test condition 'pos == 10' failed for %s.", name);
        return false;
    }

    return true;
}

typedef enum ReadError
{
    READ_NO_ERROR,
    READ_OPEN_ERROR = 1u << 0u,
    READ_CONTENT_ERROR = 1u << 1u,
    READ_ANY_ERROR = READ_OPEN_ERROR | READ_CONTENT_ERROR,
} ReadError;

static const size_t gReadTestMaxDisplayLength = 128;

const char* pTestReadFiles[] = {
    "TestDoc.txt",
    "TestDoc.txt", // different content check
    "TestDoc.txt", // different size check
    "NonExisting.txt",
};

uint64_t gTestReadFilesIds[] = {
    UINT64_MAX,
    UINT64_MAX,
    UINT64_MAX,
    UINT64_MAX,
};

const ReadError pReadErrors[] = {
    READ_NO_ERROR,
    READ_CONTENT_ERROR,
    READ_CONTENT_ERROR,
    READ_OPEN_ERROR,
};

const char* pTestReadContent[] = {
    "Hello World! This is an example for the The Forge's Zip FileSystem.",
    "Hello World! This is an example for the The Forge's Zip FileSystem!",
    "Hello World! This is an example for the The Forge's Zip.",
    "",
};
const size_t pTestReadContentSize[] = {
    strlen(pTestReadContent[0]),
    strlen(pTestReadContent[1]),
    strlen(pTestReadContent[2]),
    strlen(pTestReadContent[3]),
};

static bool testReadFile(const char* testName, ReadError expectedError, ResourceDirectory rd, const char* pFileName,
                         const char* expectedContent, size_t expectedContentSize)
{
    ASSERT(testName && pFileName);
    FileStream fStream = {};
    FileMode   mode = FM_READ;
    bool       noerr = true;

    if (!fsOpenStreamFromPath(rd, pFileName, mode, &fStream))
    {
        if (!(expectedError & READ_OPEN_ERROR))
        {
            LOGF(eERROR, "\"%s\": Couldn't open file '%s' for read.", testName, pFileName);
            return false;
        }
        return true;
    }
    else if (expectedError & READ_OPEN_ERROR)
    {
        LOGF(eERROR, "\"%s\": Expected open error for '%s' file.", testName, pFileName);
        fsCloseStream(&fStream);
        return false;
    }

    if (!expectedContent)
    {
        noerr = fsCloseStream(&fStream);
        if (!noerr)
            LOGF(eERROR, "\"%s\": Failed to close read stream of '%s' file.", testName, pFileName);
        return noerr;
    }

    size_t fileSize = fsGetStreamFileSize(&fStream);

    char*  content = (char*)tf_malloc(fileSize + 1);
    size_t readBytes = fsReadFromStream(&fStream, content, fileSize);
    if (readBytes != fileSize)
    {
        LOGF(eERROR, "\"%s\": Couldn't read %ul bytes from file '%s'. Read %ul bytes instead.", testName, (unsigned long)fileSize,
             pFileName, (unsigned long)readBytes);
        noerr = false;
    }
    content[readBytes] = '\0';

    int diff = 0;
    if (readBytes < expectedContentSize)
        diff = -(int)(expectedContentSize - readBytes);
    else if (readBytes > expectedContentSize)
        diff = (int)(expectedContentSize - readBytes);

    if (diff == 0 && readBytes != 0)
    {
        diff = memcmp(content, expectedContent, readBytes);
    }

    if (diff != 0)
    {
        if (!(expectedError & READ_CONTENT_ERROR))
        {
            LOGF(eERROR, "\"%s\": Content of file '%s' differs from expected. Diff result: %d.", testName, pFileName, diff);
            if (readBytes < gReadTestMaxDisplayLength && expectedContentSize < gReadTestMaxDisplayLength)
            {
                LOGF(eINFO, "\"%s\": File content:\n%s", testName, content);
                LOGF(eINFO, "\"%s\": Expected content:\n%s", testName, expectedContent);
            }
            noerr = false;
        }
    }
    else if (expectedError & READ_CONTENT_ERROR)
    {
        LOGF(eERROR, "\"%s\": Expected content difference for '%s' file.", testName, pFileName);
        noerr = false;
    }

    tf_free(content);
    if (!fsCloseStream(&fStream))
    {
        LOGF(eERROR, "\"%s\": Failed to close read stream for file '%s'.", testName, pFileName);

        noerr = false;
    }
    return noerr;
}

static bool testReadFileByIndex(const char* testName, ReadError expectedError, IFileSystem* pIO, uint64_t fileIndex,
                                const char* expectedContent, size_t expectedContentSize)
{
    ASSERT(testName);
    FileStream fStream = {};
    bool       noerr = true;

    if (!pIO->OpenByUid(pIO, fileIndex, FM_READ, &fStream))
    {
        if (!(expectedError & READ_OPEN_ERROR))
        {
            LOGF(eERROR, "\"%s\": Couldn't open file '%llu' for read.", testName, (unsigned long long)fileIndex);
            return false;
        }
        return true;
    }
    else if (expectedError & READ_OPEN_ERROR)
    {
        LOGF(eERROR, "\"%s\": Expected open error for '%llu' file.", testName, (unsigned long long)fileIndex);
        fsCloseStream(&fStream);
        return false;
    }

    if (!expectedContent)
    {
        noerr = fsCloseStream(&fStream);
        if (!noerr)
            LOGF(eERROR, "\"%s\": Failed to close read stream of '%llu' file.", testName, (unsigned long long)fileIndex);
        return noerr;
    }

    size_t fileSize = fsGetStreamFileSize(&fStream);

    char*  content = (char*)tf_malloc(fileSize + 1);
    size_t readBytes = fsReadFromStream(&fStream, content, fileSize);
    if (readBytes != fileSize)
    {
        LOGF(eERROR, "\"%s\": Couldn't read %ul bytes from file '%llu'. Read %ul bytes instead.", testName, (unsigned long)fileSize,
             (unsigned long long)fileIndex, (unsigned long)readBytes);
        noerr = false;
    }
    content[readBytes] = '\0';

    int diff = 0;
    if (readBytes < expectedContentSize)
        diff = -(int)(expectedContentSize - readBytes);
    else if (readBytes > expectedContentSize)
        diff = (int)(expectedContentSize - readBytes);

    if (diff == 0 && readBytes != 0)
    {
        diff = memcmp(content, expectedContent, readBytes);
    }

    if (diff != 0)
    {
        if (!(expectedError & READ_CONTENT_ERROR))
        {
            LOGF(eERROR, "\"%s\": Content of file '%llu' differs from expected. Diff result: %d.", testName, (unsigned long long)fileIndex,
                 diff);
            if (readBytes < gReadTestMaxDisplayLength && expectedContentSize < gReadTestMaxDisplayLength)
            {
                LOGF(eINFO, "\"%s\": File content:\n%s", testName, content);
                LOGF(eINFO, "\"%s\": Expected content:\n%s", testName, expectedContent);
            }
            noerr = false;
        }
    }
    else if (expectedError & READ_CONTENT_ERROR)
    {
        LOGF(eERROR, "\"%s\": Expected content difference for '%llu' file.", testName, (unsigned long long)fileIndex);
        noerr = false;
    }

    tf_free(content);
    if (!fsCloseStream(&fStream))
    {
        LOGF(eERROR, "\"%s\": Failed to close read stream for file '%llu'.", testName, (unsigned long long)fileIndex);

        noerr = false;
    }
    return noerr;
}

extern "C"
{
    FORGE_API bool fsIsNormalizedPath(const char* path, char separator);

    FORGE_API size_t fsNormalizePathContinue(const char* nextPath, char separator, const char* beg, char* cur, const char* end);

    static inline size_t fsNormalizePath(const char* path, char separator, char* output)
    {
        return fsNormalizePathContinue(path, separator, output, output, (char*)UINTPTR_MAX);
    }

    FORGE_API bool fsMergeDirAndFileName(const char* prePath, const char* postPath, char separator, size_t outputSize, char* output);
}

#define TEST_CHECK(x) \
    ASSERT(x);        \
    if (!(x))         \
    return false

static bool runFsRoutinesTests()
{
    struct Test0
    {
        const char* path;
        char        separator;
        bool        isNormalized;
    };

    Test0 tests0[] = {
        { ".\\", '\\', false },
        { ".", '/', true },
        { "1/..", '/', false },
        { "1/../", '/', false },
        { "", '/', true },
        { "\\", '/', false },
        { "/", '\\', false },
        { "C:/", '/', true },
        { "C:/..", '/', true },
        { "/..", '/', true },
        { "gg", '/', true },
        { "\\\\", '\\', false },
        { "../../../1", '/', true },
        { "./", '/', false },
        { "./.", '/', false },
        { "/..1/2", '/', true },
        { ".1./.../3/4../g/_/+/$/%/ /~~12..", '/', true },
    };

    for (size_t i = 0; i < TF_ARRAY_COUNT(tests0); ++i)
    {
        Test0 test = tests0[i];
        TEST_CHECK(fsIsNormalizedPath(test.path, test.separator) == test.isNormalized);
    }

    struct Test1
    {
        const char* src;
        const char* result;
    };

    Test1 tests1[] = {
        { ".", "." }, { "./", "." }, { "1/..", "." }, { "1/../", "." }, { "", "" },
    };

    char p[FS_MAX_PATH];
    for (size_t i = 0; i < TF_ARRAY_COUNT(tests1); ++i)
    {
        Test1 test = tests1[i];
        fsNormalizePath(test.src, '/', p);
        TEST_CHECK(fsIsNormalizedPath(p, '/'));
        TEST_CHECK(strcmp(p, test.result) == 0);
    }

    struct Test2
    {
        const char* src1;
        const char* src2;
        const char* result;
        const char  separator;
    };

    Test2 tests2[] = {
        { "1/23sdf//.///4234", "../../43///55", "1/43/55", '/' },
        { "/1/2", "../../../../", "/../..", '/' },
        { "/1/2/", "/../../..", "\\..", '\\' },
        { "C:/1/2", "../../../../..", "C:\\..\\..\\..", '\\' },
        { "C:/1/2", "/../../../../../", "C:/../../..", '/' },
    };

    for (size_t i = 0; i < TF_ARRAY_COUNT(tests2); ++i)
    {
        Test2 test = tests2[i];
        TEST_CHECK(fsMergeDirAndFileName(test.src1, test.src2, test.separator, sizeof p, p));
        TEST_CHECK(fsIsNormalizedPath(p, test.separator));
        TEST_CHECK(strcmp(p, test.result) == 0);
    }

    return true;
};

#undef TEST_CHECK

static bool runArchiveReadTests()
{
    ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pReadErrors));
    ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pTestReadContent));
    ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pTestReadContentSize));

    for (size_t i = 0; i < SIZEOF_ARR(pTestReadFiles); ++i)
    {
        static char testNameFormat[] = "Read test #%lu";
        char        testName[256] = { 0 };
        snprintf(testName, 256, testNameFormat, (unsigned long)i);

        ReadError   expectedError = pReadErrors[i];
        const char* pFileName = pTestReadFiles[i];
        const char* pContent = pTestReadContent[i];
        size_t      contentSize = pTestReadContentSize[i];

        if (!testReadFile(testName, expectedError, RD_ARCHIVE_TEST, pFileName, pContent, contentSize))
            return false;
    }

    // Fill in array
    for (size_t i = 0; i < SIZEOF_ARR(pTestReadFiles); ++i)
    {
        ReadError   expectedError = pReadErrors[i];
        const char* pFileName = pTestReadFiles[i];

        bool noerr = gArchiveFileSystem.GetFileUid(&gArchiveFileSystem, RD_ARCHIVE_TEST, pFileName, &gTestReadFilesIds[i]);

        if (!noerr && expectedError != READ_OPEN_ERROR)
        {
            LOGF(eERROR, "Failed to find entry '%s'", pFileName);
            return false;
        }
        else if (noerr && expectedError == READ_OPEN_ERROR)
        {
            LOGF(eERROR, "Found entry '%s', but it shouldn't exist.", pFileName);
            return false;
        }
    }

    // Test
    for (size_t i = 0; i < SIZEOF_ARR(pTestReadFiles); ++i)
    {
        static char testNameFormat[] = "Read test by index #%lu";
        char        testName[256] = { 0 };
        snprintf(testName, 256, testNameFormat, (unsigned long)i);

        ReadError   expectedError = pReadErrors[i];
        uint64_t    fileIndex = gTestReadFilesIds[i];
        const char* pContent = pTestReadContent[i];
        size_t      contentSize = pTestReadContentSize[i];

        if (!testReadFileByIndex(testName, expectedError, &gArchiveFileSystem, fileIndex, pContent, contentSize))
            return false;
    }

    return true;
}

// TODO should we put open/close function calls out of a benchmark scope?
// This benchmark compares read speed of various compression formats
static bool runArchiveBenchmark()
{
    bool success = true;
    for (int i = 0; i < gNumbersFileCount; ++i)
    {
        int64_t startTime = getUSec(true);

        FileStream fs;
        if (!fsIoOpenStreamFromPath(&gArchiveFileSystem, RD_ARCHIVE_TEST, gNumbersFileNames[i], FM_READ, &fs))
            return false;

        ssize_t expected_size = fsGetStreamFileSize(&fs);

        size_t size = (size_t)expected_size;

        while (size)
        {
            uint8_t buffer[4 * 1024];
            size_t  rs = fsReadFromStream(&fs, buffer, sizeof(buffer));
            size -= rs;
            if (rs == 0)
                break;
        }

        fsCloseStream(&fs);

        int64_t endTime = getUSec(true);

        if (size != 0)
        {
            LOGF(eERROR,
                 "Archive read test for file %s is failed after reading %zu out "
                 "of %zu (%s)",
                 gNumbersFileNames[i], (size_t)expected_size - size, (size_t)expected_size, humanReadableTime(endTime - startTime).str);
            success = false;
        }
        else
        {
            LOGF(eINFO, "Archive read test for file %s took %s", gNumbersFileNames[i], humanReadableTime(endTime - startTime).str);
        }
    }

    return success;
}

static bool runArchiveSeekTests()
{
    bool testSuccess = true;
    for (int i = 0; i < gNumbersFileCount; ++i)
    {
        bool success = true;

        int64_t startTime = getUSec(true);

        FileStream fs;
        if (!fsIoOpenStreamFromPath(&gArchiveFileSystem, RD_ARCHIVE_TEST, gNumbersFileNames[i], FM_READ, &fs))
            return false;

        ssize_t size = fsGetStreamFileSize(&fs);

        while ((size -= 8 * 1024) > 0)
        {
            if (!fsSeekStream(&fs, SBO_START_OF_FILE, size))
            {
                success = false;
                break;
            }

            uint64_t number;
            if (fsReadFromStream(&fs, &number, 8) != 8)
            {
                success = false;
                break;
            }

            if (number != (uint64_t)size / 8)
            {
                success = false;
                break;
            }
        }

        fsCloseStream(&fs);

        int64_t endTime = getUSec(true);

        LOGF(eINFO, "Archive seek test for file %s %s. %s", gNumbersFileNames[i], success ? "success" : "failure",
             humanReadableTime(endTime - startTime).str);

        if (!success)
            testSuccess = false;
    }

    return testSuccess;
}

static bool runTests()
{
    if (!testFindStream("forward", fsFindStream))
        return false;
    LOGF(eINFO, "Forward find succeded.");
    if (!testFindStream("backward", fsFindReverseStream))
        return false;
    LOGF(eINFO, "Backward find succeded.");

    if (!runFsRoutinesTests())
        return false;
    if (!runArchiveReadTests())
        return false;
    if (!runArchiveBenchmark())
        return false;
    if (!runArchiveSeekTests())
        return false;

    LOGF(eINFO, "Archive tests succeded.");

    return true;
}

class FileSystemUnitTest: public IApp
{
public:
    bool Init()
    {
        // testFindReverseStream();
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        // Generate sphere vertex buffer
        if (!pSpherePoints)
        {
            generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution, gSphereDiameter);
        }

        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "ZipFiles");

        struct ArchiveOpenDesc openDesc = { 0 };

        openDesc.mmap = true;

        if (!fsArchiveOpen(RD_OTHER_FILES, pZipReadFile, &openDesc, &gArchiveFileSystem))
        {
            LOGF(eERROR, "Failed to open zip file for read.");
            return false;
        }

        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

        // Load files processed by asset pipeline and zipped
        fsSetPathForResourceDir(&gArchiveFileSystem, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(&gArchiveFileSystem, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(&gArchiveFileSystem, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(&gArchiveFileSystem, RM_CONTENT, RD_ARCHIVE_TEST, "");

        gVertexLayoutDefault.mBindingCount = 1;
        gVertexLayoutDefault.mAttribCount = 3;
        gVertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        gVertexLayoutDefault.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        gVertexLayoutDefault.mAttribs[0].mBinding = 0;
        gVertexLayoutDefault.mAttribs[0].mLocation = 0;
        gVertexLayoutDefault.mAttribs[0].mOffset = 0;
        gVertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        gVertexLayoutDefault.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
        gVertexLayoutDefault.mAttribs[1].mBinding = 0;
        gVertexLayoutDefault.mAttribs[1].mLocation = 1;
        gVertexLayoutDefault.mAttribs[1].mOffset = sizeof(float) * 3;
        gVertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        gVertexLayoutDefault.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
        gVertexLayoutDefault.mAttribs[2].mBinding = 0;
        gVertexLayoutDefault.mAttribs[2].mLocation = 2;
        gVertexLayoutDefault.mAttribs[2].mOffset = sizeof(float) * 3 + sizeof(uint32_t);

        if (!runTests())
            LOGF(eERROR, "Couldn't run tests successfully.");

        FileStream textFileHandle = {};
        if (!fsOpenStreamFromPath(RD_ARCHIVE_TEST, pTextFileName[0], FM_READ, &textFileHandle))
        {
            LOGF(LogLevel::eERROR, "\"%s\": ERROR in searching for file.", pTextFileName[0]);
            return false;
        }

        ssize_t textFileSize = fsGetStreamFileSize(&textFileHandle);
        if (textFileSize < 0)
        {
            LOGF(LogLevel::eERROR, "\"%s\": Error in reading file.", pTextFileName[0]);
            return false;
        }
        size_t bytesRead = fsReadBstringFromStream(&textFileHandle, &gText, textFileSize);
        fsCloseStream(&textFileHandle);

        if (bytesRead != (size_t)textFileSize)
        {
            LOGF(LogLevel::eERROR, "\"%s\": Error in reading file.", pTextFileName[0]);
            return false;
        }

        // Actual diffs and tests
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.mD3D11Supported = true;
        settings.mGLESSupported =
            false; // We're using bitwise operations to unpack vertex attributes on the vertex shader and GLES 2.0 doesn't support it
        initRenderer(GetName(), &settings, &pRenderer);

        // check for init success
        if (!pRenderer)
            return false;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);

        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            QueryPoolDesc queryPoolDesc = {};
            queryPoolDesc.mType = QUERY_TYPE_OCCLUSION;
            queryPoolDesc.mQueryCount = gMaxOcclusionQueries;
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                addQueryPool(pRenderer, &queryPoolDesc, &pOcclusionQueryPool[i]);
            }
        }

        initResourceLoaderInterface(pRenderer);

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer;
        if (!initFontSystem(&fontRenderDesc))
            return false; // report?

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer;
        initUserInterface(&uiRenderDesc);

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        // Gpu profiler can only be added after initProfile.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        // Load Zip file texture
        TextureLoadDesc textureDescZip = {};
        textureDescZip.pFileName = pCubeTextureName[0];
        textureDescZip.ppTexture = &pZipTexture[0];
        // Textures representing color should be stored in SRGB or HDR format
        textureDescZip.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        addResource(&textureDescZip, NULL);

        // Loads Skybox Textures
        for (int i = 0; i < 6; ++i)
        {
            TextureLoadDesc textureDesc = {};
            textureDesc.pFileName = pSkyboxImageFileNames[i];
            textureDesc.ppTexture = &pSkyboxTextures[i];
            // Textures representing color should be stored in SRGB or HDR format
            textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&textureDesc, NULL);
        }

        GeometryLoadDesc loadDesc = {};
        loadDesc.pFileName = pModelFileName[0];
        loadDesc.ppGeometry = &pMesh;
        loadDesc.pVertexLayout = &gVertexLayoutDefault;
        addResource(&loadDesc, NULL);

        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_NEAREST,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pSamplerSkybox);

        // Generate Cuboid Vertex Buffer

        float widthCube = 1.0f;
        float heightCube = 1.0f;
        float depthCube = 1.0f;

        float CubePoints[] = {
            // Position				        //Normals				//TexCoords
            -widthCube, -heightCube, -depthCube, 0.0f,        0.0f,       -1.0f,       0.0f,       0.0f,        widthCube,  -heightCube,
            -depthCube, 0.0f,        0.0f,       -1.0f,       1.0f,       0.0f,        widthCube,  heightCube,  -depthCube, 0.0f,
            0.0f,       -1.0f,       1.0f,       1.0f,        widthCube,  heightCube,  -depthCube, 0.0f,        0.0f,       -1.0f,
            1.0f,       1.0f,        -widthCube, heightCube,  -depthCube, 0.0f,        0.0f,       -1.0f,       0.0f,       1.0f,
            -widthCube, -heightCube, -depthCube, 0.0f,        0.0f,       -1.0f,       0.0f,       0.0f,

            -widthCube, -heightCube, depthCube,  0.0f,        0.0f,       1.0f,        0.0f,       0.0f,        widthCube,  -heightCube,
            depthCube,  0.0f,        0.0f,       1.0f,        1.0f,       0.0f,        widthCube,  heightCube,  depthCube,  0.0f,
            0.0f,       1.0f,        1.0f,       1.0f,        widthCube,  heightCube,  depthCube,  0.0f,        0.0f,       1.0f,
            1.0f,       1.0f,        -widthCube, heightCube,  depthCube,  0.0f,        0.0f,       1.0f,        0.0f,       1.0f,
            -widthCube, -heightCube, depthCube,  0.0f,        0.0f,       1.0f,        0.0f,       0.0f,

            -widthCube, heightCube,  depthCube,  -1.0f,       0.0f,       0.0f,        1.0f,       0.0f,        -widthCube, heightCube,
            -depthCube, -1.0f,       0.0f,       0.0f,        1.0f,       1.0f,        -widthCube, -heightCube, -depthCube, -1.0f,
            0.0f,       0.0f,        0.0f,       1.0f,        -widthCube, -heightCube, -depthCube, -1.0f,       0.0f,       0.0f,
            0.0f,       1.0f,        -widthCube, -heightCube, depthCube,  -1.0f,       0.0f,       0.0f,        0.0f,       0.0f,
            -widthCube, heightCube,  depthCube,  -1.0f,       0.0f,       0.0f,        1.0f,       0.0f,

            widthCube,  heightCube,  depthCube,  1.0f,        0.0f,       0.0f,        1.0f,       0.0f,        widthCube,  heightCube,
            -depthCube, 1.0f,        0.0f,       0.0f,        1.0f,       1.0f,        widthCube,  -heightCube, -depthCube, 1.0f,
            0.0f,       0.0f,        0.0f,       1.0f,        widthCube,  -heightCube, -depthCube, 1.0f,        0.0f,       0.0f,
            0.0f,       1.0f,        widthCube,  -heightCube, depthCube,  1.0f,        0.0f,       0.0f,        0.0f,       0.0f,
            widthCube,  heightCube,  depthCube,  1.0f,        0.0f,       0.0f,        1.0f,       0.0f,

            -widthCube, -heightCube, -depthCube, 0.0f,        -1.0f,      0.0f,        0.0f,       1.0f,        widthCube,  -heightCube,
            -depthCube, 0.0f,        -1.0f,      0.0f,        1.0f,       1.0f,        widthCube,  -heightCube, depthCube,  0.0f,
            -1.0f,      0.0f,        1.0f,       0.0f,        widthCube,  -heightCube, depthCube,  0.0f,        -1.0f,      0.0f,
            1.0f,       0.0f,        -widthCube, -heightCube, depthCube,  0.0f,        -1.0f,      0.0f,        0.0f,       0.0f,
            -widthCube, -heightCube, -depthCube, 0.0f,        -1.0f,      0.0f,        0.0f,       1.0f,

            -widthCube, heightCube,  -depthCube, 0.0f,        1.0f,       0.0f,        0.0f,       1.0f,        widthCube,  heightCube,
            -depthCube, 0.0f,        1.0f,       0.0f,        1.0f,       1.0f,        widthCube,  heightCube,  depthCube,  0.0f,
            1.0f,       0.0f,        1.0f,       0.0f,        widthCube,  heightCube,  depthCube,  0.0f,        1.0f,       0.0f,
            1.0f,       0.0f,        -widthCube, heightCube,  depthCube,  0.0f,        1.0f,       0.0f,        0.0f,       0.0f,
            -widthCube, heightCube,  -depthCube, 0.0f,        1.0f,       0.0f,        0.0f,       1.0f
        };

        uint64_t       cubiodDataSize = 288 * sizeof(float);
        BufferLoadDesc cubiodVbDesc = {};
        cubiodVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        cubiodVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        cubiodVbDesc.mDesc.mSize = cubiodDataSize;
        cubiodVbDesc.pData = CubePoints;
        cubiodVbDesc.ppBuffer = &pZipTextureVertexBuffer;
        addResource(&cubiodVbDesc, NULL);

        // Generate sky box vertex buffer
        float skyBoxPoints[] = {
            10.0f,  -10.0f, -10.0f, 6.0f, // -z
            -10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
            -10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

            -10.0f, -10.0f, 10.0f,  2.0f, //-x
            -10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
            -10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

            10.0f,  -10.0f, -10.0f, 1.0f, //+x
            10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
            10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

            -10.0f, -10.0f, 10.0f,  5.0f, // +z
            -10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
            10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

            -10.0f, 10.0f,  -10.0f, 3.0f, //+y
            10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
            10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

            10.0f,  -10.0f, 10.0f,  4.0f, //-y
            10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
            -10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
        };

        uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
        BufferLoadDesc skyboxVbDesc = {};
        skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
        skyboxVbDesc.pData = skyBoxPoints;
        skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
        addResource(&skyboxVbDesc, NULL);

        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(UniformBlock);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;

        uint64_t       sphereDataSize = gNumberOfSpherePoints * sizeof(float);
        BufferLoadDesc sphereVbDesc = {};
        sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        sphereVbDesc.mDesc.mSize = sphereDataSize;
        sphereVbDesc.pData = pSpherePoints;
        sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
        addResource(&sphereVbDesc, NULL);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
            addResource(&ubDesc, NULL);
        }

        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);

        //--------------------------------

        // Gui for Showing the Text of the File
        uiCreateComponent("Opened Document", &guiDesc, &pGui_TextData);

        LabelWidget textWidget;
        luaRegisterWidget(uiCreateComponentWidget(pGui_TextData, (const char*)gText.data, &textWidget, WIDGET_TYPE_LABEL));

        //--------------------------------

        UIComponentDesc guiOcclusionDesc = {};
        guiOcclusionDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * .3f);
        uiCreateComponent("Occlusion Test", &guiOcclusionDesc, &pGui_OcclusionData);

        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            DynamicTextWidget occlusionRedWidget;
            occlusionRedWidget.pText = &gOcclusionbstr;
            occlusionRedWidget.pColor = &gOccluion1Color;
            UIWidget* pOcclusionWidget =
                uiCreateComponentWidget(pGui_OcclusionData, "Sphere Occlusion:", &occlusionRedWidget, WIDGET_TYPE_DYNAMIC_TEXT);
            luaRegisterWidget(pOcclusionWidget);
        }
        //--------------------------------

        // CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
        vec3 camPos{ 48.0f, 48.0f, 20.0f };
        vec3 lookAt{ 0 };

        pCameraController = initFpsCameraController(camPos, lookAt);

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = NULL; // Disable Virtual Joystick
        if (!initInputSystem(&inputDesc))
            return false;

        // App Actions
        InputActionDesc actionDesc = { DefaultInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       this };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::EXIT, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           requestShutdown();
                           return true;
                       } };
        addInputAction(&actionDesc);
        InputActionCallback onUIInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }
            return true;
        };

        typedef bool (*CameraInputHandler)(InputActionContext * ctx, DefaultInputActions::DefaultInputAction action);
        static CameraInputHandler onCameraInput = [](InputActionContext* ctx, DefaultInputActions::DefaultInputAction action)
        {
            if (*(ctx->pCaptured))
            {
                float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
                switch (action)
                {
                case DefaultInputActions::ROTATE_CAMERA:
                    pCameraController->onRotate(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA:
                    pCameraController->onMove(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                    pCameraController->onMoveY(delta[0]);
                    break;
                default:
                    break;
                }
            }
            return true;
        };
        actionDesc = { DefaultInputActions::CAPTURE_INPUT,
                       [](InputActionContext* ctx)
                       {
                           setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                           return true;
                       },
                       NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::ROTATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA_VERTICAL); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           if (!uiWantTextInput())
                               pCameraController->resetView();
                           return true;
                       } };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameIndex = 0;
        waitForAllResourceLoads();

        return true;
    }

    void Exit()
    {
        if (pSpherePoints)
        {
            tf_free(pSpherePoints);
            pSpherePoints = nullptr;
        }

        fsArchiveClose(&gArchiveFileSystem);

        bdestroy(&gText);

        exitInputSystem();

        exitCameraController(pCameraController);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pProjViewUniformBuffer[i]);
        }

        removeResource(pSphereVertexBuffer);
        removeResource(pSkyboxVertexBuffer);
        removeResource(pZipTextureVertexBuffer);

        for (uint i = 0; i < 6; ++i)
            removeResource(pSkyboxTextures[i]);

        // remove loaded zip test texture
        removeResource(pZipTexture[0]);

        // remove loaded zip test models
        removeResource(pMesh);
        pMesh = NULL;

        removeSampler(pRenderer, pSamplerSkybox);

        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);

        exitResourceLoaderInterface(pRenderer);
        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                removeQueryPool(pRenderer, pOcclusionQueryPool[i]);
            }
        }
        removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTextures);
        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFrameUniforms);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetTextures);
        removeDescriptorSet(pRenderer, pDescriptorSetFrameUniforms);
    }

    void prepareDescriptorSets()
    {
        // Skybox
        {
            // Prepare descriptor sets
            DescriptorData params[7] = {};
            params[0].pName = "RightText";
            params[0].ppTextures = &pSkyboxTextures[0];
            params[1].pName = "LeftText";
            params[1].ppTextures = &pSkyboxTextures[1];
            params[2].pName = "TopText";
            params[2].ppTextures = &pSkyboxTextures[2];
            params[3].pName = "BotText";
            params[3].ppTextures = &pSkyboxTextures[3];
            params[4].pName = "FrontText";
            params[4].ppTextures = &pSkyboxTextures[4];
            params[5].pName = "BackText";
            params[5].ppTextures = &pSkyboxTextures[5];

            params[6].pName = "ZipTexture";
            params[6].ppTextures = pZipTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetTextures, 7, params);
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData params[1] = {};
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &pProjViewUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetFrameUniforms, 1, params);
        }
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;

            if (!addDepthBuffer())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        uiLoad.mHeight = mSettings.mHeight;
        uiLoad.mWidth = mSettings.mWidth;
        uiLoad.mLoadType = pReloadDesc->mType;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        initScreenshotInterface(pRenderer, pGraphicsQueue);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
            removeRenderTarget(pRenderer, pDepthBuffer);
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

        pCameraController->update(deltaTime);

        /************************************************************************/
        // Scene Update
        /****************************************************************/
        // update camera with time
        mat4 viewMat = pCameraController->getViewMatrix();

        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

        // Projection and View Matrix
        gUniformData.mProjectView = projMat * viewMat;

        // Model Matrix
        mat4 trans = mat4::translation(vec3(15.0f, 0.0f, 22.0f));
        mat4 scale = mat4::scale(vec3(5.0f));
        gUniformData.mModelMatCapsule = trans * scale;

        //********************************
        // Uniform buffer data of the cube with zip texture
        //********************************

        mat4 mTranslationMat_Zip;
        mat4 mScaleMat_Zip;

        mTranslationMat_Zip = mat4::translation(vec3(10.5f, 1.0f, 3.0f));
        mScaleMat_Zip = mat4::scale(vec3(10.5f));
        gUniformData.mModelMatCube = mTranslationMat_Zip * mScaleMat_Zip;

        gUniformData.mModelOcclusion = mat4::translation(vec3(-10.5f, 2.5f, -10.0f)) * mat4::scale(vec3(10.5f));
        viewMat.setTranslation(vec3(0));
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        RenderTarget*     pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            QueryData occlusionData = {};
            getQueryData(pRenderer, pOcclusionQueryPool[gFrameIndex], gOccTestOccuionSphereMaxIndex, &occlusionData);
            const uint64_t maxOcclusion = occlusionData.mOcclusionCounts;
            getQueryData(pRenderer, pOcclusionQueryPool[gFrameIndex], gOccTestOccuionSphereIndex, &occlusionData);
            const uint64_t testOcclusion = occlusionData.mOcclusionCounts;
            float          visibility = ((maxOcclusion == 0.0f) ? 0.0f : (((float)testOcclusion) / ((float)maxOcclusion))) * 100.0f;
            bformat(&gOcclusionbstr, "Visible texels %u | Total texels %u | (%f%%)", testOcclusion, maxOcclusion, visibility);
        }

        resetCmdPool(pRenderer, elem.pCmdPool);

        // Update uniform buffers
        BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        memcpy(viewProjCbv.pMappedData, &gUniformData, sizeof(gUniformData));
        endUpdateResource(&viewProjCbv);

        // simply record the screen cleaning command
        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            cmdResetQuery(cmd, pOcclusionQueryPool[gFrameIndex], 0, gMaxOcclusionQueries);
        }

        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        // draw occlusion max queries
        QueryDesc occlusionQueryDesc = {};

        const uint32_t sphereStride = sizeof(float) * 6;
        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw occlsuion Max");
            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
            cmdBindPipeline(cmd, pOcclusionMax);

            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetFrameUniforms);

            occlusionQueryDesc.mIndex = gOccTestOccuionSphereMaxIndex;
            cmdBeginQuery(cmd, pOcclusionQueryPool[gFrameIndex], &occlusionQueryDesc);
            cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer, &sphereStride, NULL);
            cmdDraw(cmd, gNumberOfSpherePoints / 6, 0);
            cmdEndQuery(cmd, pOcclusionQueryPool[gFrameIndex], &occlusionQueryDesc);

            cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
        }

        //// draw skybox
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw skybox");
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
        cmdBindPipeline(cmd, pPipelineSkybox);

        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetFrameUniforms);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetTextures);

        const uint32_t skyboxStride = sizeof(float) * 4;
        cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &skyboxStride, NULL);
        cmdDraw(cmd, 36, 0);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        ////// draw Zip Model
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Zip Model");
        cmdBindPipeline(cmd, pBasicPipeline);

        cmdBindVertexBuffer(cmd, 1, &pMesh->pVertexBuffers[0], &pMesh->mVertexStrides[0], NULL);
        cmdBindIndexBuffer(cmd, pMesh->pIndexBuffer, pMesh->mIndexType, 0);
        cmdDrawIndexed(cmd, pMesh->mIndexCount, 0, 0);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        ////draw Cube with Zip texture
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Zip File Texture");
        cmdBindPipeline(cmd, pZipTexturePipeline);

        const uint32_t cubeStride = sizeof(float) * 8;
        cmdBindVertexBuffer(cmd, 1, &pZipTextureVertexBuffer, &cubeStride, NULL);
        cmdDraw(cmd, 36, 0);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        //// draw occlusion test
        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Occlusion Test");
            cmdBindPipeline(cmd, pOcclusionTest);

            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetFrameUniforms);

            occlusionQueryDesc.mIndex = gOccTestOccuionSphereIndex;
            cmdBeginQuery(cmd, pOcclusionQueryPool[gFrameIndex], &occlusionQueryDesc);
            cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer, &sphereStride, NULL);
            cmdDraw(cmd, gNumberOfSpherePoints / 6, 0);
            cmdEndQuery(cmd, pOcclusionQueryPool[gFrameIndex], &occlusionQueryDesc);

            cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
        }

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
        {
            bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_LOAD };
            cmdBindRenderTargets(cmd, &bindRenderTargets);

            gFrameTimeDraw.mFontColor = 0xff00ffff;
            gFrameTimeDraw.mFontSize = 18.0f;
            gFrameTimeDraw.mFontID = gFontID;
            float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
            cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

            cmdDrawUserInterface(cmd);
            cmdBindRenderTargets(cmd, NULL);
        }
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
        if (pRenderer->pGpu->mSettings.mOcclusionQueries)
        {
            cmdResolveQuery(cmd, pOcclusionQueryPool[gFrameIndex], 0, gMaxOcclusionQueries);
        }
        endCmd(cmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);
        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;

        /// Exit if quick exit is enabled
#if ZIP_TESTS_QUICK_EXIT
        mSettings.mQuit = true;
#endif
    }

    const char* GetName() { return "12_ZipFileSystem"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addRootSignatures()
    {
        Shader* shaders[] = { pSkyboxShader, pBasicShader, pZipTextureShader, pOcclusionShader };

        const char*       pStaticSamplers[] = { "uSampler0" };
        RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
        skyboxRootDesc.mStaticSamplerCount = 1;
        skyboxRootDesc.ppStaticSamplerNames = pStaticSamplers;
        skyboxRootDesc.ppStaticSamplers = &pSamplerSkybox;
        skyboxRootDesc.mShaderCount = 4;
        skyboxRootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignature);
    }

    void removeRootSignatures() { removeRootSignature(pRenderer, pRootSignature); }

    void addShaders()
    {
        ShaderLoadDesc skyShader = {};
        skyShader.mStages[0].pFileName = "skybox.vert";
        skyShader.mStages[1].pFileName = "skybox.frag";
        ShaderLoadDesc basicShader = {};
        basicShader.mStages[0].pFileName = "basic.vert";
        basicShader.mStages[1].pFileName = "basic.frag";
        ShaderLoadDesc occlusionShader = {};
        occlusionShader.mStages[0].pFileName = "occlusion.vert";
        occlusionShader.mStages[1].pFileName = "occlusion.frag";
        ShaderLoadDesc zipTextureShader = {};
        zipTextureShader.mStages[0].pFileName = "zipTexture.vert";
        zipTextureShader.mStages[1].pFileName = "zipTexture.frag";

        addShader(pRenderer, &skyShader, &pSkyboxShader);
        addShader(pRenderer, &basicShader, &pBasicShader);
        addShader(pRenderer, &zipTextureShader, &pZipTextureShader);
        addShader(pRenderer, &occlusionShader, &pOcclusionShader);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pOcclusionShader);
        removeShader(pRenderer, pBasicShader);
        removeShader(pRenderer, pSkyboxShader);
        removeShader(pRenderer, pZipTextureShader);
    }

    void addPipelines()
    {
        DepthStateDesc noDepthStateDesc = {};
        BlendStateDesc blendStateNoWriteDesc{};

        RasterizerStateDesc rasterizerStateFrontDesc = {};
        rasterizerStateFrontDesc.mFillMode = FILL_MODE_SOLID;
        rasterizerStateFrontDesc.mCullMode = CULL_MODE_FRONT;

        // layout and pipeline for zip model draw
        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        RasterizerStateDesc cubeRasterizerStateDesc = {};
        cubeRasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        RasterizerStateDesc binCubeRasterizerStateDesc = {};
        binCubeRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pBasicShader;
        pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
        pipelineSettings.pRasterizerState = &binCubeRasterizerStateDesc;
        addPipeline(pRenderer, &desc, &pBasicPipeline);

        // layout and pipeline for skybox draw
        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 1;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;

        pipelineSettings.pVertexLayout = &vertexLayout;
        pipelineSettings.pDepthState = NULL;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.pShaderProgram = pSkyboxShader;
        addPipeline(pRenderer, &desc, &pPipelineSkybox);

        // layout and pipeline for the zip test texture

        vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 3;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
        vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[2].mBinding = 0;
        vertexLayout.mAttribs[2].mLocation = 2;
        vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pRasterizerState = &cubeRasterizerStateDesc;
        pipelineSettings.pShaderProgram = pZipTextureShader;
        addPipeline(pRenderer, &desc, &pZipTexturePipeline);

        vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

        desc.mType = PIPELINE_TYPE_GRAPHICS;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pVertexLayout = &vertexLayout;
        pipelineSettings.pRasterizerState = &rasterizerStateFrontDesc;
        pipelineSettings.pShaderProgram = pOcclusionShader;
        addPipeline(pRenderer, &desc, &pOcclusionTest);

        pipelineSettings.pDepthState = &noDepthStateDesc;
        pipelineSettings.pBlendState = &blendStateNoWriteDesc;
        pipelineSettings.pRasterizerState = &rasterizerStateFrontDesc;
        addPipeline(pRenderer, &desc, &pOcclusionMax);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pZipTexturePipeline);
        removePipeline(pRenderer, pPipelineSkybox);
        removePipeline(pRenderer, pBasicPipeline);
        removePipeline(pRenderer, pOcclusionTest);
        removePipeline(pRenderer, pOcclusionMax);
    }

    bool addDepthBuffer()
    {
        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue.depth = 0.0f;
        depthRT.mClearValue.stencil = 0;
        depthRT.mDepth = 1;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        return pDepthBuffer != NULL;
    }
};

DEFINE_APPLICATION_MAIN(FileSystemUnitTest)
