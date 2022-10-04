/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include <stdio.h>

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/utility.h"

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/minizip/mz.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/minizip/mz_strm.h"


//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Application/Interfaces/IInput.h"
//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define SIZEOF_ARR(x) sizeof(x)/sizeof(x[0])

/// Demo structures

struct UniformBlock
{
	CameraMatrix mProjectView;
	mat4 mModelMatCapsule;
	mat4 mModelMatCube;
};

struct Vertex
{
	float3 mPosition;
	float3 mNormal;
	float2 mUV;
};

const uint32_t gImageCount = 3;
ProfileToken   gGpuProfileToken;
Renderer*      pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pBasicShader = NULL;
Pipeline* pBasicPipeline = NULL;

Shader*        pSkyboxShader = NULL;
Buffer*        pSkyboxVertexBuffer = NULL;
Pipeline*      pPipelineSkybox = NULL;
RootSignature* pRootSignature = NULL;
Sampler*       pSamplerSkybox = NULL;
Texture*       pSkyboxTextures[6];

//Zip File Test Texture
Texture*  pZipTexture[1];
Shader*   pZipTextureShader = NULL;
Buffer*   pZipTextureVertexBuffer = NULL;
Pipeline* pZipTexturePipeline = NULL;

Buffer* pProjViewUniformBuffer[gImageCount] = { NULL };

DescriptorSet* pDescriptorSetFrameUniforms = NULL;
DescriptorSet* pDescriptorSetTextures = NULL;

uint32_t gFrameIndex = 0;

int          gNumberOfBasicPoints;
int          gNumberOfCubiodPoints;
UniformBlock gUniformData;

ICameraController* pCameraController = NULL;

const char* pSkyboxImageFileNames[] = { "Skybox/Skybox_right1",  "Skybox/Skybox_left2",  "Skybox/Skybox_top3",
										"Skybox/Skybox_bottom4", "Skybox/Skybox_front5", "Skybox/Skybox_back6" };

const char* pCubeTextureName[] = { "TheForge" };

const char* pTextFileName[] = { "TestDoc.txt" };

const ResourceDirectory RD_ZIP_TEXT_ENCRYPTED = RD_MIDDLEWARE_0;

const ResourceDirectory RD_ZIP_TEXT_UNENCRYPTED = RD_MIDDLEWARE_1;

// RM_CONTENT can be a read only directory on some platforms,
// so we use RM_SAVE_0 for saving instead
const ResourceDirectory RD_ZIP_WRITE_DIRECTORY = RD_MIDDLEWARE_2;

const ResourceDirectory RD_ZIP_TEXT_WRITE = RD_MIDDLEWARE_3;
const ResourceDirectory RD_ZIP_TEXT_WRITE_COMPLEX_PATH = RD_MIDDLEWARE_4;

const ResourceDirectory RD_ZIP_TEXT_WRITE_ENCRYPTED = RD_MIDDLEWARE_5;
const ResourceDirectory RD_ZIP_TEXT_WRITE_ENCRYPTED_COMPLEX_PATH = RD_MIDDLEWARE_6;

const ResourceDirectory RD_ZIP_TEXT_WRITE_ONLY = RD_MIDDLEWARE_7;
const ResourceDirectory RD_ZIP_TEXT_WRITE_ONLY_COMPLEX_PATH = RD_MIDDLEWARE_8;

const ResourceDirectory RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED = RD_MIDDLEWARE_9;
const ResourceDirectory RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED_COMPLEX_PATH = RD_MIDDLEWARE_10;


const char* pModelFileName[] = { "matBall.bin" };

FontDrawDesc gFrameTimeDraw; 
uint32_t     gFontID = 0; 

UIComponent* pGui_TextData = NULL;

UIComponent* pGui_ZipData = NULL;

//Zip file for testing
const char* pZipReadFile = "28-ZipFileSystem.zip";
const char* pZipReadEncryptedFile = "28-ZipFileSystemEncrypted.zip";
const char* pZipWriteFile = "28-ZipFileSystemWrite.zip";
const char* pZipWriteEncryptedFile = "28-ZipFileSystemWriteEncrypted.zip";
const char* pZipWriteOnlyFile = "28-ZipFileSystemWriteOnly.zip";
const char* pZipWriteOnlyEncryptedFile = "28-ZipFileSystemWriteOnlyEncrypted.zip";

const char* pZipPassword = "12345";

bstring gText = bempty();

//structures for loaded model
Geometry*    pMesh;
VertexLayout gVertexLayoutDefault = {};

const char* pMTunerOut = "testout.txt";
IFileSystem gZipReadFileSystem = { 0 };
IFileSystem gZipReadEncryptedFileSystem = { 0 };

IFileSystem gZipWriteFileSystem = { 0 };
IFileSystem gZipWriteEncryptedFileSystem = { 0 };
IFileSystem gZipWriteOnlyFileSystem = { 0 };
IFileSystem gZipWriteOnlyEncryptedFileSystem = { 0 };

typedef bool(*findStreamCb)(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t *pPosition);

static bool testFindStream(const char* name, findStreamCb findCb) {
	
	char txt[] = "THIS IS A TEST TEXT";
	const char pattern[] = "TEST";
	FileStream stream;
	fsOpenStreamFromMemory(txt, sizeof(txt), FM_READ, false, &stream);
	if (findCb == fsFindReverseStream)
		fsSeekStream(&stream, SBO_END_OF_FILE, 0);
	ssize_t pos = INT_MAX;
	findCb(&stream, pattern, sizeof(pattern) - 1, INT_MAX, &pos);
	fsCloseStream(&stream);
	if (pos != 10) {
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
}ReadError;

static const size_t gReadTestMaxDisplayLength = 128;

const char* pTestReadFiles[] = {
	"TestDoc.txt",
	"TestDoc.txt", // different content check
	"TestDoc.txt", // different size check
	"NonExisting.txt",
	// Encrypted
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
	UINT64_MAX,
	UINT64_MAX,
	UINT64_MAX,
	UINT64_MAX,
};

const char* pReadPasswords[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	// Encrypted
	pZipPassword,
	pZipPassword,
	pZipPassword,
	pZipPassword,
};

const ReadError	        pReadErrors[] = {
	READ_NO_ERROR,
	READ_CONTENT_ERROR,
	READ_CONTENT_ERROR,
	READ_OPEN_ERROR,
	READ_NO_ERROR,
	READ_CONTENT_ERROR,
	READ_CONTENT_ERROR,
	READ_OPEN_ERROR,
};
const ResourceDirectory pTestReadDirs[] = {
	RD_ZIP_TEXT_UNENCRYPTED,
	RD_ZIP_TEXT_UNENCRYPTED,
	RD_ZIP_TEXT_UNENCRYPTED,
	RD_ZIP_TEXT_UNENCRYPTED,
	RD_ZIP_TEXT_ENCRYPTED,
	RD_ZIP_TEXT_ENCRYPTED,
	RD_ZIP_TEXT_ENCRYPTED,
	RD_ZIP_TEXT_ENCRYPTED,
};

const char* pTestReadContent[] = {
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem!",
	"Hello World! This is an example for the The Forge's Zip.",
	"",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem!",
	"Hello World! This is an example for the The Forge's Zip.",
	""
};
const size_t pTestReadContentSize[] = {
	strlen(pTestReadContent[0]),
	strlen(pTestReadContent[1]),
	strlen(pTestReadContent[2]),
	strlen(pTestReadContent[3]),
	strlen(pTestReadContent[4]),
	strlen(pTestReadContent[5]),
	strlen(pTestReadContent[6]),
	strlen(pTestReadContent[7]),
};

static bool testReadFile(const char* testName, ReadError expectedError, 
	ResourceDirectory rd, const char * pFileName, const char* pFilePassword,
	const char* expectedContent, size_t expectedContentSize)
{

	ASSERT(testName && pFileName);
	FileStream fStream = {};
	FileMode mode = FM_READ;
	bool noerr = true;

	if (!fsOpenStreamFromPath(rd, pFileName, mode, pFilePassword, &fStream))
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
	
	char* content = (char*)tf_malloc(fileSize + 1);
	size_t readBytes = fsReadFromStream(&fStream, content, fileSize);
	if (readBytes != fileSize)
	{
		LOGF(eERROR, "\"%s\": Couldn't read %ul bytes from file '%s'. Read %ul bytes instead.",
			testName, (unsigned long)fileSize, pFileName, (unsigned long)readBytes);
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

			LOGF(eERROR, "\"%s\": Content of file '%s' differs from expected. Diff result: %d.",
				testName, pFileName, diff);
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

static bool testReadFileByIndex(const char* testName, ReadError expectedError,
	IFileSystem* pIO, uint64_t fileIndex, const char* pFilePassword,
	const char* expectedContent, size_t expectedContentSize)
{

	ASSERT(testName);
	FileStream fStream = {};
	FileMode mode = FM_READ;
	bool noerr = true;

	if (!fsOpenZipEntryByIndex(pIO, fileIndex, mode, pFilePassword, &fStream))
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

	char* content = (char*)tf_malloc(fileSize + 1);
	size_t readBytes = fsReadFromStream(&fStream, content, fileSize);
	if (readBytes != fileSize)
	{
		LOGF(eERROR, "\"%s\": Couldn't read %ul bytes from file '%llu'. Read %ul bytes instead.",
			testName, (unsigned long)fileSize, (unsigned long long)fileIndex, (unsigned long)readBytes);
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

			LOGF(eERROR, "\"%s\": Content of file '%llu' differs from expected. Diff result: %d.",
				testName, (unsigned long long)fileIndex, diff);
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

const char* pTestWriteFiles[] = {
	"TestDoc.txt",
	"Foo/Bar/TestDoc.txt",
	"TestDoc.txt",
	// Encrypted
	"TestDoc.txt",
	"Foo/Bar/TestDoc.txt",
	"TestDoc.txt",
	// Write only
	"TestDoc.txt",
	"Foo/Bar/TestDoc.txt",
	"TestDoc.txt",
	// Encrypted
	"TestDoc.txt",
	"Foo/Bar/TestDoc.txt",
	"TestDoc.txt",
};
const char* pWritePasswords[] = {
	NULL,
	NULL,
	NULL,
	// Encrypted
	pZipPassword,
	pZipPassword,
	pZipPassword,
	// write only
	NULL,
	NULL,
	NULL,
	// Encrypted
	pZipPassword,
	pZipPassword,
	pZipPassword,
};
const ResourceDirectory pTestWriteDirs[] = {
	RD_ZIP_TEXT_WRITE,
	RD_ZIP_TEXT_WRITE,
	RD_ZIP_TEXT_WRITE_COMPLEX_PATH,
	RD_ZIP_TEXT_WRITE_ENCRYPTED,
	RD_ZIP_TEXT_WRITE_ENCRYPTED,
	RD_ZIP_TEXT_WRITE_ENCRYPTED_COMPLEX_PATH,
	RD_ZIP_TEXT_WRITE_ONLY,
	RD_ZIP_TEXT_WRITE_ONLY,
	RD_ZIP_TEXT_WRITE_ONLY_COMPLEX_PATH,
	RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED,
	RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED,
	RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED_COMPLEX_PATH,
};
const char* pTestWriteContent[] = {
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
	"Hello World! This is an example for the The Forge's Zip FileSystem.",
};
const size_t pTestWriteContentSize[] = {
	strlen(pTestWriteContent[0]),
	strlen(pTestWriteContent[1]),
	strlen(pTestWriteContent[2]),
	strlen(pTestWriteContent[3]),
	strlen(pTestWriteContent[4]),
	strlen(pTestWriteContent[5]),
	strlen(pTestWriteContent[6]),
	strlen(pTestWriteContent[7]),
	strlen(pTestWriteContent[8]),
	strlen(pTestWriteContent[9]),
	strlen(pTestWriteContent[10]),
	strlen(pTestWriteContent[11]),
};

const bool gIsWriteTestWriteOnly[] = {
	false,
	false,
	false,
	false,
	false,
	false,
	true,
	true,
	true,
	true,
	true,
	true,
};


static bool testWriteFile(const char* testName, ResourceDirectory rd, 
	const char * pFileName, const char* pFilePassword, const char* content, size_t contentSize, bool isWriteOnly)
{
	FileStream fStream = {};
	FileMode mode = FM_WRITE;
	bool noerr = true;

	if (!fsOpenStreamFromPath(rd, pFileName, mode, pFilePassword, &fStream))
	{
		LOGF(eERROR, "\"%s\": Coudln't open file '%s' for write.", testName, pFileName);
		return false;
	}

	size_t writtenBytes = fsWriteToStream(&fStream, content, contentSize);

	if (writtenBytes != contentSize) {
		LOGF(eERROR, "\"%s\": Couldn't write %ul bytes into file '%s'. Wrote %ul bytes instead.", 
			testName, (unsigned long)contentSize, pFileName, (unsigned long)writtenBytes);
		noerr = false;
	}

	if (!fsCloseStream(&fStream))
	{
		LOGF(eERROR, "\"%s\": Failed to close write stream for file '%s'.", testName, pFileName);
		noerr = false;
	}

	if (noerr && !isWriteOnly)
	{
		char modifiedName[4096] = { 0 };
		snprintf(modifiedName, 4096, "%s(check content)", testName);
		noerr = testReadFile(modifiedName, READ_NO_ERROR, rd, pFileName, pFilePassword, content, contentSize);
	}
	
	return noerr;
}

static bool runReadZipTests()
{
	ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pTestReadDirs));
	ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pReadPasswords));
	ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pReadErrors));
	ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pTestReadContent));
	ASSERT(SIZEOF_ARR(pTestReadFiles) == SIZEOF_ARR(pTestReadContentSize));
	
	for (size_t i = 0; i < SIZEOF_ARR(pTestReadFiles); ++i)
	{
		static char testNameFormat[] = "Read test #%lu";
		char testName[256] = {0};
		snprintf(testName, 256, testNameFormat, (unsigned long)i);

		ReadError         expectedError = pReadErrors[i];
		const char*       pFileName = pTestReadFiles[i];
		const char*       pFilePassword = pReadPasswords[i];
		ResourceDirectory rd = pTestReadDirs[i];
		const char*       pContent = pTestReadContent[i];
		size_t            contentSize = pTestReadContentSize[i];
		
		if (!testReadFile(testName, expectedError, rd, pFileName, pFilePassword, pContent, contentSize))
			return false;
	}

	// Fill in array
	for (size_t i = 0; i < SIZEOF_ARR(pTestReadFiles)/2; ++i)
	{
		ReadError         expectedError = pReadErrors[i];
		const char*       pFileName = pTestReadFiles[i];
		ResourceDirectory rd = pTestReadDirs[i];

		bool noerr = fsFetchZipEntryIndex(&gZipReadFileSystem, rd, pFileName, &gTestReadFilesIds[i]);
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
	for (size_t i = 0; i < SIZEOF_ARR(pTestReadFiles) / 2; ++i)
	{
		static char testNameFormat[] = "Read test by index #%lu";
		char testName[256] = { 0 };
		snprintf(testName, 256, testNameFormat, (unsigned long)i);

		ReadError         expectedError = pReadErrors[i];
		uint64_t          fileIndex = gTestReadFilesIds[i];
		const char*       pFilePassword = pReadPasswords[i];
		const char*       pContent = pTestReadContent[i];
		size_t            contentSize = pTestReadContentSize[i];

		if (!testReadFileByIndex(testName, expectedError, &gZipReadFileSystem, fileIndex, pFilePassword, pContent, contentSize))
			return false;
	}

	// Fill in array
	for (size_t i = SIZEOF_ARR(pTestReadFiles) / 2; i < SIZEOF_ARR(pTestReadFiles); ++i)
	{
		ReadError         expectedError = pReadErrors[i];
		const char*       pFileName = pTestReadFiles[i];
		ResourceDirectory rd = pTestReadDirs[i];

		bool noerr = fsFetchZipEntryIndex(&gZipReadEncryptedFileSystem, rd, pFileName, &gTestReadFilesIds[i]);
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
	for (size_t i = SIZEOF_ARR(pTestReadFiles) / 2; i < SIZEOF_ARR(pTestReadFiles); ++i)
	{
		static char testNameFormat[] = "Read test by index #%lu";
		char testName[256] = { 0 };
		snprintf(testName, 256, testNameFormat, (unsigned long)i);

		ReadError         expectedError = pReadErrors[i];
		uint64_t          fileIndex = gTestReadFilesIds[i];
		const char*       pFilePassword = pReadPasswords[i];
		const char*       pContent = pTestReadContent[i];
		size_t            contentSize = pTestReadContentSize[i];

		if (!testReadFileByIndex(testName, expectedError, &gZipReadEncryptedFileSystem, fileIndex, pFilePassword, pContent, contentSize))
			return false;
	}

	return true;
}

static bool runWriteZipTests()
{
	ASSERT(SIZEOF_ARR(pTestWriteFiles) == SIZEOF_ARR(pWritePasswords));
	ASSERT(SIZEOF_ARR(pTestWriteFiles) == SIZEOF_ARR(pTestWriteDirs));
	ASSERT(SIZEOF_ARR(pTestWriteFiles) == SIZEOF_ARR(pTestWriteContent));
	ASSERT(SIZEOF_ARR(pTestWriteFiles) == SIZEOF_ARR(pTestWriteContentSize));
	// Test read and write files
	for (size_t i = 0; i < SIZEOF_ARR(pTestWriteFiles); ++i)
	{
		static char testNameFormat[] = "Write test #%lu";
		char testName[256] = { 0 };
		snprintf(testName, 256, testNameFormat, (unsigned long)i);

		const char*       pFileName = pTestWriteFiles[i];
		const char*       pFilePassword = pWritePasswords[i];
		ResourceDirectory rd = pTestWriteDirs[i];
		const char*       pContent = pTestWriteContent[i];
		size_t            contentSize = pTestWriteContentSize[i];
		bool              isWriteOnly = gIsWriteTestWriteOnly[i];

		if (!testWriteFile(testName, rd, pFileName, pFilePassword, pContent, contentSize, isWriteOnly))
			return false;
	}
	return true;
}

static bool runTests()
{
	if (!testFindStream("forward", fsFindStream))
		return false;
	LOGF(eINFO, "Forward find succeded.");
	if (!testFindStream("backward", fsFindReverseStream))
		return false;
	LOGF(eINFO, "Backward find succeded.");

	if (!runReadZipTests())
		return false;
	// Test reads, but keep file handles always open
	fsOpenZipFile(&gZipReadFileSystem);
	fsOpenZipFile(&gZipReadEncryptedFileSystem);
	if (!runReadZipTests())
	{
		fsCloseZipFile(&gZipReadFileSystem);
		fsCloseZipFile(&gZipReadEncryptedFileSystem);
		return false;
	}
	fsCloseZipFile(&gZipReadFileSystem);
	fsCloseZipFile(&gZipReadEncryptedFileSystem);
	LOGF(eINFO, "Read zip tests succeded.");

	if (!runWriteZipTests())
		return false;
	uint64_t writtenFiles;
	if (!fsEntryCountZipFile(&gZipWriteFileSystem, &writtenFiles))
	{
		LOGF(eERROR, "Failed to count number of written files.");
		return false;
	}
	if (writtenFiles != 3)
	{
		LOGF(eERROR, "Number of written files is incorrect.");
		return false;
	}
	if (!fsEntryCountZipFile(&gZipWriteEncryptedFileSystem, &writtenFiles))
	{
		LOGF(eERROR, "Failed to count number of written files to encrypted zip.");
		return false;
	}
	if (writtenFiles != 3)
	{
		LOGF(eERROR, "Number of written files is incorrect for encrypted zip.");
		return false;
	}
	//FileStream stream;
	//fsOpenZipEntryByIndex(&gZipWriteFileSystem, 1, FM_WRITE, NULL, &stream);
	LOGF(eINFO, "Write zip tests succeded.");



	return true;
}

class FileSystemUnitTest : public IApp
{
	public:
	bool Init()
	{
		//testFindReverseStream();

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "ZipFiles");
		
		if (!initZipFileSystem(RD_OTHER_FILES, pZipReadFile, FM_READ, NULL, &gZipReadFileSystem))
		{
			LOGF(eERROR, "Failed to open zip file for read.");
			return false;
		}

		if (!initZipFileSystem(RD_OTHER_FILES, pZipReadEncryptedFile, FM_READ, NULL, &gZipReadEncryptedFileSystem))
		{
			LOGF(eERROR, "Failed to open encrypted zip file for read.");
			return false;
		}


		// Set write directory
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_ZIP_WRITE_DIRECTORY, "ZipWriteFiles");


		// Delete zip file before opening
		char fPath[FS_MAX_PATH];
		fsAppendPathComponent(fsGetResourceDirectory(RD_ZIP_WRITE_DIRECTORY), pZipWriteFile, fPath);
		remove(fPath);
		if (!initZipFileSystem(RD_ZIP_WRITE_DIRECTORY, pZipWriteFile, FM_READ_WRITE, NULL, &gZipWriteFileSystem))
		{
			LOGF(eERROR, "Failed to open zip file for write.");
			return false;
		}

		// Delete file before opening
		fsAppendPathComponent(fsGetResourceDirectory(RD_ZIP_WRITE_DIRECTORY), pZipWriteEncryptedFile, fPath);
		remove(fPath);

		if (!initZipFileSystem(RD_ZIP_WRITE_DIRECTORY, pZipWriteEncryptedFile, FM_READ_WRITE, NULL, &gZipWriteEncryptedFileSystem))
		{
			LOGF(eERROR, "Failed to open encrypted zip file for write.");
			return false;
		}

		// Delete file before opening
		fsAppendPathComponent(fsGetResourceDirectory(RD_ZIP_WRITE_DIRECTORY), pZipWriteOnlyFile, fPath);
		remove(fPath);

		if (!initZipFileSystem(RD_ZIP_WRITE_DIRECTORY, pZipWriteOnlyFile, FM_WRITE, NULL, &gZipWriteOnlyFileSystem))
		{
			LOGF(eERROR, "Failed to open encrypted zip file for write.");
			return false;
		}

		// Delete file before opening
		fsAppendPathComponent(fsGetResourceDirectory(RD_ZIP_WRITE_DIRECTORY), pZipWriteOnlyEncryptedFile, fPath);
		remove(fPath);

		if (!initZipFileSystem(RD_ZIP_WRITE_DIRECTORY, pZipWriteOnlyEncryptedFile, FM_WRITE, NULL, &gZipWriteOnlyEncryptedFileSystem))
		{
			LOGF(eERROR, "Failed to open encrypted zip file for write.");
			return false;
		}

		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		fsSetPathForResourceDir(&gZipReadEncryptedFileSystem, RM_CONTENT, RD_ZIP_TEXT_ENCRYPTED, "");
		// Load files processed by asset pipeline and zipped
		fsSetPathForResourceDir(&gZipReadFileSystem, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(&gZipReadFileSystem, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(&gZipReadEncryptedFileSystem, RM_CONTENT, RD_FONTS, "Fonts");
		
		fsSetPathForResourceDir(&gZipReadFileSystem, RM_CONTENT, RD_ZIP_TEXT_UNENCRYPTED, "");

		fsSetPathForResourceDir(&gZipWriteFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE, "");
		fsSetPathForResourceDir(&gZipWriteFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_COMPLEX_PATH, "Very/Complex/Path");

		fsSetPathForResourceDir(&gZipWriteEncryptedFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_ENCRYPTED, "");
		fsSetPathForResourceDir(&gZipWriteEncryptedFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_ENCRYPTED_COMPLEX_PATH, "Very/Complex/Path");

		fsSetPathForResourceDir(&gZipWriteOnlyFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_ONLY, "");
		fsSetPathForResourceDir(&gZipWriteOnlyFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_ONLY_COMPLEX_PATH, "Very/Complex/Path");

		fsSetPathForResourceDir(&gZipWriteOnlyEncryptedFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED, "");
		fsSetPathForResourceDir(&gZipWriteOnlyEncryptedFileSystem, RM_CONTENT, RD_ZIP_TEXT_WRITE_ONLY_ENCRYPTED_COMPLEX_PATH, "Very/Complex/Path");


		gVertexLayoutDefault.mAttribCount = 3;
		gVertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutDefault.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutDefault.mAttribs[0].mBinding = 0;
		gVertexLayoutDefault.mAttribs[0].mLocation = 0;
		gVertexLayoutDefault.mAttribs[0].mOffset = 0;
		gVertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutDefault.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutDefault.mAttribs[1].mBinding = 0;
		gVertexLayoutDefault.mAttribs[1].mLocation = 1;
		gVertexLayoutDefault.mAttribs[1].mOffset = sizeof(float) * 3;
		gVertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutDefault.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutDefault.mAttribs[2].mBinding = 0;
		gVertexLayoutDefault.mAttribs[2].mLocation = 2;
		gVertexLayoutDefault.mAttribs[2].mOffset = sizeof(float) * 6;

		if (!runTests())
			LOGF(eERROR, "Couldn't run tests successfully.");

		FileStream textFileHandle = {};
		if (!fsOpenStreamFromPath(RD_ZIP_TEXT_ENCRYPTED, pTextFileName[0], FM_READ, pZipPassword, &textFileHandle))
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
		settings.mGLESSupported = true;
		initRenderer(GetName(), &settings, &pRenderer);

		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		font.pFontPassword = pZipPassword; 
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

		//Load Zip file texture
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
		loadDesc.pFilePassword = nullptr;
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
			//Position				        //Normals				//TexCoords
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

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
			10.0f,  -10.0f, -10.0f, 6.0f,    // -z
			-10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
			-10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

			-10.0f, -10.0f, 10.0f,  2.0f,    //-x
			-10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
			-10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

			10.0f,  -10.0f, -10.0f, 1.0f,    //+x
			10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
			10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

			-10.0f, -10.0f, 10.0f,  5.0f,    // +z
			-10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
			10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

			-10.0f, 10.0f,  -10.0f, 3.0f,    //+y
			10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
			10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

			10.0f,  -10.0f, 10.0f,  4.0f,    //-y
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

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}
		waitForAllResourceLoads();

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);

		//--------------------------------

		//Gui for Showing the Text of the File
		uiCreateComponent("Opened Document", &guiDesc, &pGui_TextData);

		LabelWidget textWidget;
		luaRegisterWidget(uiCreateComponentWidget(pGui_TextData, (const char*)gText.data, &textWidget, WIDGET_TYPE_LABEL));

		//--------------------------------

		//CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3 camPos{ 48.0f, 48.0f, 20.0f };
		vec3 lookAt{ 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		inputDesc.mDisableVirtualJoystick = true; 
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; }};
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
			{
				uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			}
			return true;
		};

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0;
		return true;
	}

	void Exit()
	{
		// Close the Zip file
		exitZipFileSystem(&gZipWriteOnlyEncryptedFileSystem);
		exitZipFileSystem(&gZipWriteOnlyFileSystem);

		exitZipFileSystem(&gZipWriteEncryptedFileSystem);
		exitZipFileSystem(&gZipWriteFileSystem);

		exitZipFileSystem(&gZipReadEncryptedFileSystem);
		exitZipFileSystem(&gZipReadFileSystem);

		bdestroy(&gText);

		exitInputSystem();

		exitCameraController(pCameraController);

		exitProfiler();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
		}

		removeResource(pSkyboxVertexBuffer);
		removeResource(pZipTextureVertexBuffer);

		for (uint i = 0; i < 6; ++i)
			removeResource(pSkyboxTextures[i]);

		//remove loaded zip test texture
		removeResource(pZipTexture[0]);

		//remove loaded zip test models
		removeResource(pMesh);

		removeSampler(pRenderer, pSamplerSkybox);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTextures);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
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

		for (uint32_t i = 0; i < gImageCount; ++i)
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

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		// Projection and View Matrix
		gUniformData.mProjectView = projMat * viewMat;

		//Model Matrix
		mat4 trans = mat4::translation(vec3(15.0f, 0.0f, 22.0f));
		mat4 scale = mat4::scale(vec3(5.0f));
		gUniformData.mModelMatCapsule = trans * scale;

		//********************************
		//Uniform buffer data of the cube with zip texture
		//********************************

		mat4 mTranslationMat_Zip;
		mat4 mScaleMat_Zip;

		mTranslationMat_Zip = mat4::translation(vec3(10.5f, 1.0f, 3.0f));
		mScaleMat_Zip = mat4::scale(vec3(10.5f));
		gUniformData.mModelMatCube = mTranslationMat_Zip * mScaleMat_Zip;

		viewMat.setTranslation(vec3(0));
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(UniformBlock*)viewProjCbv.pMappedData = gUniformData;
		endUpdateResource(&viewProjCbv, NULL);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		//// draw skybox
#pragma region Skybox_Draw
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
#pragma endregion

		////// draw Zip Model
#pragma region Zip_Model_Draw
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Zip Model");
		cmdBindPipeline(cmd, pBasicPipeline);

		cmdBindVertexBuffer(cmd, 1, &pMesh->pVertexBuffers[0], &pMesh->mVertexStrides[0], NULL);
		cmdBindIndexBuffer(cmd, pMesh->pIndexBuffer, pMesh->mIndexType, 0);
		cmdDrawIndexed(cmd, pMesh->mIndexCount, 0, 0);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
#pragma endregion

		////draw Cube with Zip texture
#pragma region Cube_Zip_Texture_Draw
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Zip File Texture");
		cmdBindPipeline(cmd, pZipTexturePipeline);

		const uint32_t cubeStride = sizeof(float) * 8;
		cmdBindVertexBuffer(cmd, 1, &pZipTextureVertexBuffer, &cubeStride, NULL);
		cmdDraw(cmd, 36, 0);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
#pragma endregion

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

			gFrameTimeDraw.mFontColor = 0xff00ffff;
			gFrameTimeDraw.mFontSize = 18.0f;
			gFrameTimeDraw.mFontID = gFontID;
			float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
			cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

			cmdDrawUserInterface(cmd);
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		}
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
		endCmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;

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
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addRootSignatures()
	{
		Shader* shaders[] = { pSkyboxShader, pBasicShader, pZipTextureShader };

		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = pStaticSamplers;
		skyboxRootDesc.ppStaticSamplers = &pSamplerSkybox;
		skyboxRootDesc.mShaderCount = 3;
		skyboxRootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignature);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);
	}

	void addShaders()
	{
		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		ShaderLoadDesc zipTextureShader = {};
		zipTextureShader.mStages[0] = { "zipTexture.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		zipTextureShader.mStages[1] = { "zipTexture.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &skyShader, &pSkyboxShader);
		addShader(pRenderer, &basicShader, &pBasicShader);
		addShader(pRenderer, &zipTextureShader, &pZipTextureShader);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pBasicShader);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pZipTextureShader);
	}

	void addPipelines()
	{
		//layout and pipeline for zip model draw
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc cubeRasterizerStateDesc = {};
		cubeRasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc binCubeRasterizerStateDesc = {};
		binCubeRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

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

		//layout and pipeline for skybox draw
		VertexLayout vertexLayout = {};
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

		//layout and pipeline for the zip test texture

		vertexLayout = {};
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
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pZipTexturePipeline);
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pBasicPipeline);
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 1.0f;
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
