///*
// * Copyright (c) 2017-2022 The Forge Interactive Inc.
// *
// * This file is part of The-Forge
// * (see https://github.com/ConfettiFX/The-Forge).
// *
// * Licensed to the Apache Software Foundation (ASF) under one
// * or more contributor license agreements.  See the NOTICE file
// * distributed with this work for additional information
// * regarding copyright ownership.  The ASF licenses this file
// * to you under the Apache License, Version 2.0 (the
// * "License"); you may not use this file except in compliance
// * with the License.  You may obtain a copy of the License at
// *
// *   http://www.apache.org/licenses/LICENSE-2.0
// *
// * Unless required by applicable law or agreed to in writing,
// * software distributed under the License is distributed on an
// * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// * KIND, either express or implied.  See the License for the
// * specific language governing permissions and limitations
// * under the License.
//*/

#include "../../Application/Config.h"

#ifdef ENABLE_ZIP_FILESYSTEM

#include "../ThirdParty/OpenSource/minizip/mz.h"
#include "../ThirdParty/OpenSource/minizip/mz_crypt.h"
#include "../ThirdParty/OpenSource/minizip/mz_os.h"
#include "../ThirdParty/OpenSource/minizip/mz_zip.h"
#include "../ThirdParty/OpenSource/minizip/mz_strm.h"
//#include "../../ThirdParty/OpenSource/minizip/mz_strm_mem.h"
//#include "../../ThirdParty/OpenSource/minizip/mz_strm_os.h"
//#include "../../ThirdParty/OpenSource/minizip/mz_strm_wzaes.h"


#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IMemory.h"

#define DEFAULT_COMPRESSION_METHOD MZ_COMPRESS_METHOD_DEFLATE
#define MAX_PASSWORD_LENGTH 64

// TODO: Make implementation thread safe

/***************************************************************
	Implementation details
***************************************************************/

typedef struct ZipFile
{
	void* pHandle;
	FileStream mFstream;

	uint32_t mOpenedEntries;
	ResourceDirectory mResourceDir;
	FileMode mMode;
	char mFileName[FILENAME_MAX];
	char mFilePassword[MAX_PASSWORD_LENGTH];
}ZipFile;

typedef struct ZipEntryStream
{
	size_t    mWriteCount;
	char      mEntryPath[FILENAME_MAX];
	char      mPassword[MAX_PASSWORD_LENGTH];
}ZipEntryStream;


static bool openZipEntry(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, const char* filePassword, FileStream* pOut);
static bool closeZipEntry(FileStream* pFile);
static size_t readZipEntry(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);
static size_t writeZipEntry(FileStream* pFile, const void* sourceBuffer, size_t byteCount);
static bool seekZipEntry(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);
static ssize_t tellZipEntry(const FileStream* pFile);
static ssize_t sizeofZipEntry(const FileStream* pFile);
static bool flushZipEntry(FileStream* pStream);
static bool isEndOfZipEntry(const FileStream* pFile);

static const IFileSystem gZipFileIO =
{
	openZipEntry,
	closeZipEntry,
	readZipEntry,
	writeZipEntry,
	seekZipEntry,
	tellZipEntry,
	sizeofZipEntry,
	flushZipEntry,
	isEndOfZipEntry,
	NULL,   // GetResourceMount
	NULL,   // GetPropInt64
	NULL,   // SetPropInt64
	NULL    // pUser
};

static bool isZipIO(IFileSystem* pIO)
{
	return pIO->Open == gZipFileIO.Open &&
		pIO->Close == gZipFileIO.Close &&
		pIO->Read == gZipFileIO.Read &&
		pIO->Write == gZipFileIO.Write &&
		pIO->Seek == gZipFileIO.Seek &&
		pIO->GetSeekPosition == gZipFileIO.GetSeekPosition &&
		pIO->GetFileSize == gZipFileIO.GetFileSize &&
		pIO->Flush == gZipFileIO.Flush &&
		pIO->IsAtEnd == gZipFileIO.IsAtEnd;
}

static bool forceOpenZipFile(ZipFile* pZipFile, bool firstOpen)
{
	ResourceDirectory resourceDir = pZipFile->mResourceDir;
	const char* fileName = pZipFile->mFileName;
	const char* password = pZipFile->mFilePassword;
	if (password[0] == '\0')
		password = NULL;
	FileMode mode = pZipFile->mMode;
	if (!firstOpen)
		mode |= FM_READ;
	if (mode & FM_APPEND)
		mode |= FM_READ_WRITE;

	// Need to exclude append as we need the ability to freely move cursor for zip files
	if (!fsOpenStreamFromPath(resourceDir, fileName, (mode | FM_BINARY) & ~FM_APPEND, password, &pZipFile->mFstream))
	{
		LOGF(eERROR, "Failed to open zip file %s.", fileName);
		return false;
	}
	
	if (!mz_zip_open(pZipFile->pHandle, &pZipFile->mFstream, mode))
	{
		LOGF(eERROR, "Failed to open zip handle to file %s.", fileName);
		fsCloseStream(&pZipFile->mFstream);
		return false;
	}
	return true;
}

static bool forceCloseZipFile(ZipFile* pZipFile)
{
	bool noerr = true;
	if (!mz_zip_close(pZipFile->pHandle))
		noerr = false;
	if (!fsCloseStream(&pZipFile->mFstream))
		noerr = false;
	return noerr;
}

static bool cleanupZipFile(ZipFile* pZipFile, bool result)
{
	mz_zip_delete(&pZipFile->pHandle);
	tf_free(pZipFile);
	return result;
}

bool fsOpenZipFile(IFileSystem* pIO)
{
	ASSERT(pIO && pIO->pUser);
	if (!isZipIO(pIO))
		return false;

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;

	if (pZipFile->mOpenedEntries == 0)
	{
		if (!forceOpenZipFile(pZipFile, false))
			return false;
	}
	++pZipFile->mOpenedEntries;
	return true;
}

bool fsCloseZipFile(IFileSystem* pIO)
{
	ASSERT(pIO && pIO->pUser);
	if (!isZipIO(pIO))
		return false;

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;

	// Already closed
	if (pZipFile->mOpenedEntries == 0)
	{
		LOGF(eERROR, "Double close of zip file '%s'", pZipFile->mFileName);
		return true;
	}

	--pZipFile->mOpenedEntries;
	if (pZipFile->mOpenedEntries == 0)
		return forceCloseZipFile(pZipFile);
	return true;
}

/***************************************************************
	Zip Entry
***************************************************************/

static bool openZipEntry(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, const char* filePassword, FileStream* pOut)
{
	ASSERT(pIO && pOut);
	if (mode & FM_APPEND)
		mode |= FM_WRITE;

	bool noerr = true;

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;
	void* zip = pZipFile->pHandle;
	// make sure that zip file is opened
	if (!fsOpenZipFile(pIO))
	{
		LOGF(eERROR, "Failed to open zip file, while trying to access zip entry.");
		return false;
	}

	FileStream* pFileStream;
	mz_zip_get_stream(zip, &pFileStream);

	FileMode zipMode = pFileStream->mMode;

	if ((mode & zipMode) != mode)
	{
		LOGF(eWARNING, "Trying to open zip entry '%s' in file mode mode '%s', while zip file was opened in '%s' mode.",
			fileName, fsFileModeToString(mode), fsFileModeToString(zipMode));
		fsCloseZipFile(pIO);
		return false;
	}


	//uint8_t raw = (mode & FM_BINARY) ? 1 : 0;

	void* buffer = NULL;
	size_t bufferSize = 0;

	ZipEntryStream* pEntryStream = tf_calloc(1, sizeof(ZipEntryStream));
	if (!pEntryStream)
	{
		LOGF(eERROR, "Failed to allocate memory for file entry %s in zip", fileName);
		fsCloseZipFile(pIO);
		return false;
	}

	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), fileName, pEntryStream->mEntryPath);


	if ((mode & FM_READ) || (mode & FM_APPEND))
	{
		noerr = mz_zip_locate_entry(zip, pEntryStream->mEntryPath, 0);

		// Ignore error if write mode is enabled
		if (noerr || !(mode & FM_WRITE))
		{
			if (!noerr) {
				LOGF(eWARNING, "Couldn't find file entry '%s' in zip.", pEntryStream->mEntryPath);
				tf_free(pEntryStream);
				fsCloseZipFile(pIO);
				return false;
			}

			noerr = mz_zip_entry_read_open(zip, 0, filePassword);

			if (!noerr) {
				LOGF(eERROR, "Couldn't open file entry '%s' in zip.", pEntryStream->mEntryPath);
				tf_free(pEntryStream);
				fsCloseZipFile(pIO);
				return false;
			}

			mz_zip_file* pFileInfo;
			noerr = mz_zip_entry_get_local_info(zip, &pFileInfo);
			if (!noerr) {
				LOGF(eERROR, "Couldn't get info on file entry '%s' in zip.", pEntryStream->mEntryPath);
				mz_zip_entry_close(zip);
				tf_free(pEntryStream);
				fsCloseZipFile(pIO);
				return false;
			}


			bufferSize = pFileInfo->uncompressed_size;

			if (bufferSize > 0)
			{
				buffer = tf_malloc(bufferSize);
				if (!buffer)
				{
					LOGF(eERROR, "Couldn't allocate buffer for reading zip file entry '%s'.", pEntryStream->mEntryPath);
					mz_zip_entry_close(zip);
					tf_free(pEntryStream);
					fsCloseZipFile(pIO);
					return false;
				}

				size_t bytesRead = mz_zip_entry_read(zip, buffer, bufferSize);
				if (bytesRead != bufferSize) {
					LOGF(eERROR, "Couldn't read zip file entry '%s'.", pEntryStream->mEntryPath);
					tf_free(buffer);
					mz_zip_entry_close(zip);
					tf_free(pEntryStream);
					fsCloseZipFile(pIO);
					return false;
				}
				mz_zip_entry_close(zip);
			}

			mz_zip_entry_close(zip);
		}
	}

	FileStream* pMemStream = tf_malloc(sizeof(FileStream));
	if (!pMemStream)
	{
		LOGF(eERROR, "Couldn't allocate memory for memory stream for zip file entry '%s'.", pEntryStream->mEntryPath);
		tf_free(buffer);
		tf_free(pEntryStream);
		fsCloseZipFile(pIO);
		return false;
	}
	if (!fsOpenStreamFromMemory(buffer, bufferSize, mode, true, pMemStream))
	{
		LOGF(eERROR, "Couldn't open memory stream for zip file entry '%s'.", pEntryStream->mEntryPath);
		tf_free(buffer);
		tf_free(pEntryStream);
		fsCloseZipFile(pIO);
		return false;
	}

	// We need to store password only if we are planning to write
	if ((mode & FM_WRITE) && filePassword)
	{
		size_t passwordLength = strlen(filePassword);
		if (passwordLength >= MAX_PASSWORD_LENGTH)
		{
			LOGF(eERROR, "Provided password for zip entry '%s' is too long.", fileName);
			fsCloseZipFile(pIO);
			return false;
		}
		strncpy(pEntryStream->mPassword, filePassword, passwordLength);
	}

	pOut->pIO = pIO;
	pOut->mMode = mode;
	pOut->pBase = pMemStream;
	pOut->pUser = pEntryStream;

	return true;
}

static bool flushZipEntry(FileStream* pStream)
{
	if (!(pStream->mMode & (FM_WRITE | FM_APPEND)))
		return true;

	bool noerr = true;

	FileStream* pMemStream = pStream->pBase;
	ASSERT(pMemStream);

	ZipFile* pZipFile = (ZipFile*)pStream->pIO->pUser;
	void* zip = pZipFile->pHandle;

	ZipEntryStream* pEntryStream = (ZipEntryStream*)pStream->pUser;
	const char* password = pEntryStream->mPassword;
	if (password[0] == '\0')
		password = NULL;


	FileStream* pFileStream;
	mz_zip_get_stream(zip, &pFileStream);


	mz_zip_file fileInfo = (mz_zip_file) { 0 };
	fileInfo.version_madeby = MZ_VERSION_MADEBY;
	fileInfo.compression_method = DEFAULT_COMPRESSION_METHOD;
	fileInfo.filename = pEntryStream->mEntryPath;
	fileInfo.filename_size = (uint16_t)strlen(pEntryStream->mEntryPath);
	fileInfo.uncompressed_size = fsGetStreamFileSize(pMemStream);
	if (password)
		fileInfo.aes_version = MZ_AES_VERSION;

	mz_zip_file* pFileInfo = &fileInfo;


	if (mz_zip_locate_entry(zip, pEntryStream->mEntryPath, 0))
	{
		mz_zip_entry_get_info(zip, &pFileInfo);
	}


	noerr = mz_zip_entry_write_open(zip, pFileInfo, MZ_COMPRESS_LEVEL_DEFAULT, 0, password);


	if (!noerr)
	{
		LOGF(eERROR, "Failed to open file entry '%s' for write in zip.", pEntryStream->mEntryPath);
		return false;
	}

	const void* pBuf = NULL;
	fsGetMemoryStreamBuffer(pMemStream, &pBuf);
	size_t memBufferSize = (size_t)fsGetStreamFileSize(pMemStream);
	if (memBufferSize > 0)
	{
		noerr = mz_zip_entry_write(zip, pBuf, memBufferSize) == memBufferSize;
		if (!noerr)
		{
			LOGF(eWARNING, "Failed to write %ul bytes to zip file entry '%s'.", (unsigned long)memBufferSize, pEntryStream->mEntryPath);
		}
	}
	
	
	if (!mz_zip_entry_close(zip))
	{
		LOGF(eWARNING, "Failed to close zip entry '%s'.", pEntryStream->mEntryPath);
		noerr = false;
	}


	return noerr;
}

static bool closeZipEntry(FileStream* pFile)
{	
	ASSERT(pFile && pFile->pIO);
	IFileSystem* pIO = pFile->pIO;

	bool noerr = true;
	if (!fsFlushStream(pFile))
		noerr = false;
	if (!fsCloseStream(pFile->pBase))
		noerr = false;

	tf_free(pFile->pBase);
	tf_free(pFile->pUser);

	if (!fsCloseZipFile(pIO))
		noerr = false;

	return noerr;
}

static size_t readZipEntry(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
{
	return fsReadFromStream(pFile->pBase, outputBuffer, bufferSizeInBytes);
}

static size_t writeZipEntry(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
{
	return fsWriteToStream(pFile->pBase, sourceBuffer, byteCount);
}

static bool seekZipEntry(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	return fsSeekStream(pFile->pBase, baseOffset, seekOffset);
}

static ssize_t tellZipEntry(const FileStream* pFile)
{
	return fsGetStreamSeekPosition(pFile->pBase);
}

static ssize_t sizeofZipEntry(const FileStream* pFile)
{
	return fsGetStreamFileSize(pFile->pBase);
}

static bool isEndOfZipEntry(const FileStream* pFile)
{
	return fsStreamAtEnd(pFile->pBase);
}

/***************************************************************
	Zip File System
***************************************************************/

bool initZipFileSystem(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, const char* password, IFileSystem* pOut)
{
	if (password && password[0] == '\0')
		password = NULL;

	if ((mode & FM_READ_WRITE) == FM_READ_WRITE || (mode & FM_READ_APPEND) == FM_READ_APPEND)
	{
		LOGF(eWARNING, "Simultaneous read and write for zip files is error prone. Use with cautious.");
	}

	size_t fileNameSize = strlen(fileName);
	if (fileNameSize >= FILENAME_MAX)
	{
		LOGF(eERROR, "Filename '%s' is too big.", fileName);
		return false;
	}
	size_t passwordSize = 0;
	if (password)
		passwordSize = strlen(password);
	
	if (passwordSize >= MAX_PASSWORD_LENGTH)
	{
		LOGF(eERROR, "Password for file '%s' is too big.");
		return false;
	}

	ZipFile* pZipFile = tf_calloc(1, sizeof(ZipFile));
	mz_zip_create(&pZipFile->pHandle);

	pZipFile->mOpenedEntries = 0;
	pZipFile->mResourceDir = resourceDir;
	pZipFile->mMode = mode;
	memcpy(pZipFile->mFileName, fileName, fileNameSize);
	if (password)
		memcpy(pZipFile->mFilePassword, password, passwordSize);
	if (!forceOpenZipFile(pZipFile, true))
	{
		LOGF(eERROR, "Failed to open zip file '%s'", fileName);
		return cleanupZipFile(pZipFile, false);
	}

	// Close everything and reopen when the entries are opened
	if (!forceCloseZipFile(pZipFile))
	{
		LOGF(eERROR, "Failed to close zip file '%s'", fileName);
		return cleanupZipFile(pZipFile, false);
	}

	IFileSystem system = gZipFileIO;
	system.pUser = pZipFile;
	*pOut = system;


	return true;
}

bool exitZipFileSystem(IFileSystem* pIO)
{
	ASSERT(pIO && pIO->pUser);
	if (!isZipIO(pIO))
		return false;
	ZipFile* pZipFile = (ZipFile*)pIO->pUser;
	bool noerr = true;
	if (pZipFile->mOpenedEntries != 0)
	{
		LOGF(eWARNING, "Closing zip file '%s' when there are %u opened entries left.", pZipFile->mFileName, pZipFile->mOpenedEntries);
		pZipFile->mOpenedEntries = 0;
		forceCloseZipFile(pZipFile);
	}

	return cleanupZipFile(pZipFile, noerr);
	
}

/***************************************************************
	Extra functionality
***************************************************************/

bool fsEntryCountZipFile(IFileSystem* pIO, uint64_t* pOut)
{
	ASSERT(pIO && pIO->pUser);
	if (!isZipIO(pIO))
		return false;

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;
	bool noerr = true;
	if (!fsOpenZipFile(pIO))
		return false;

	uint64_t entryCount;
	if (!mz_zip_get_number_entry(pZipFile->pHandle, &entryCount))
		noerr = false;
	else
		*pOut = entryCount;
	
	if (!fsCloseZipFile(pIO))
		noerr = false;

	return noerr;
}


bool fsOpenZipEntryByIndex(IFileSystem* pIO, uint64_t index, FileMode mode, const char* filePassword, FileStream* pOut)
{
	ASSERT(pIO && pOut);
	if (!isZipIO(pIO))
		return false;

	ASSERT((mode & (FM_WRITE | FM_APPEND)) == 0);

	bool noerr = true;

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;
	void* zip = pZipFile->pHandle;

	FileMode zipMode = pZipFile->mMode;

	if ((mode & zipMode & ~FM_BINARY) != (mode & ~FM_BINARY))
	{
		LOGF(eWARNING, "Trying to open zip entry at index '%llu' in file mode mode '%s', while zip file was opened in '%s' mode.",
			(unsigned long long)index, fsFileModeToString(mode), fsFileModeToString(zipMode));
		fsCloseZipFile(pIO);
		return false;
	}

	// make sure that zip file is opened
	if (!fsOpenZipFile(pIO))
	{
		LOGF(eERROR, "Failed to open zip file, while trying to access zip entry.");
		return false;
	}

	FileStream* pFileStream;
	mz_zip_get_stream(zip, &pFileStream);

	//uint8_t raw = (mode & FM_BINARY) ? 1 : 0;

	void* buffer = NULL;
	size_t bufferSize = 0;

	noerr = mz_zip_goto_entry(zip, index);
	if (!noerr)
	{
		LOGF(eWARNING, "Failed to find file entry at index '%llu' in zip", (unsigned long long)index);
		fsCloseZipFile(pIO);
		return false;
	}

	ZipEntryStream* pEntryStream = tf_calloc(1, sizeof(ZipEntryStream));
	if (!pEntryStream)
	{
		LOGF(eERROR, "Failed to allocate memory for file entry at index '%llu' in zip", (unsigned long long)index);
		fsCloseZipFile(pIO);
		return false;
	}


	mz_zip_file* pFileInfo;
	noerr = mz_zip_entry_get_info(zip, &pFileInfo);
	if (!noerr) {
		LOGF(eERROR, "Couldn't get info on file entry '%s' in zip.", pEntryStream->mEntryPath);
		mz_zip_entry_close(zip);
		tf_free(pEntryStream);
		fsCloseZipFile(pIO);
		return false;
	}

	ASSERT(pFileInfo->filename_size < FILENAME_MAX);
	memcpy(pEntryStream->mEntryPath, pFileInfo->filename, pFileInfo->filename_size);
	pEntryStream->mEntryPath[pFileInfo->filename_size] = '\0';



	bufferSize = pFileInfo->uncompressed_size;

	noerr = mz_zip_entry_read_open(zip, 0, filePassword);

	if (!noerr) {
		LOGF(eERROR, "Couldn't open file entry '%s' in zip.", pEntryStream->mEntryPath);
		tf_free(pEntryStream);
		fsCloseZipFile(pIO);
		return false;
	}

	if (bufferSize > 0)
	{
		buffer = tf_malloc(bufferSize);
		if (!buffer)
		{
			LOGF(eERROR, "Couldn't allocate buffer for reading zip file entry '%s'.", pEntryStream->mEntryPath);
			mz_zip_entry_close(zip);
			tf_free(pEntryStream);
			fsCloseZipFile(pIO);
			return false;
		}

		size_t bytesRead = mz_zip_entry_read(zip, buffer, bufferSize);
		if (bytesRead != bufferSize) {
			LOGF(eERROR, "Couldn't read zip file entry '%s'.", pEntryStream->mEntryPath);
			tf_free(buffer);
			mz_zip_entry_close(zip);
			tf_free(pEntryStream);
			fsCloseZipFile(pIO);
			return false;
		}
		mz_zip_entry_close(zip);
	}

	mz_zip_entry_close(zip);
		

	

	FileStream* pMemStream = tf_malloc(sizeof(FileStream));
	if (!pMemStream)
	{
		LOGF(eERROR, "Couldn't allocate memory for memory stream for zip file entry '%s'.", pEntryStream->mEntryPath);
		tf_free(buffer);
		tf_free(pEntryStream);
		fsCloseZipFile(pIO);
		return false;
	}
	if (!fsOpenStreamFromMemory(buffer, bufferSize, mode, true, pMemStream))
	{
		LOGF(eERROR, "Couldn't open memory stream for zip file entry '%s'.", pEntryStream->mEntryPath);
		tf_free(buffer);
		tf_free(pEntryStream);
		fsCloseZipFile(pIO);
		return false;
	}

	pOut->pIO = pIO;
	pOut->mMode = mode;
	pOut->pBase = pMemStream;
	pOut->pUser = pEntryStream;

	return true;
	
}

bool fsFetchZipEntryIndex(IFileSystem* pIO, ResourceDirectory resourceDir, const char* pFileName, uint64_t* pOut)
{
	ASSERT(pIO && pIO->pUser);
	if (!isZipIO(pIO))
		return false;

	// make sure that zip file is opened
	if (!fsOpenZipFile(pIO))
	{
		LOGF(eERROR, "Failed to open zip file, while trying to fetch zip entry filename.");
		return false;
	}

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;
	void* zip = pZipFile->pHandle;

	char filePath[FS_MAX_PATH];
	fsAppendPathComponent(fsGetResourceDirectory(resourceDir), pFileName, filePath);


	bool noerr = mz_zip_locate_entry(zip, filePath, 0);
	
	if (noerr)
		*pOut = (size_t)mz_zip_get_entry(zip);


	if (!fsCloseZipFile(pIO))
	{
		LOGF(eERROR, "Failed to close zip file, while trying to fetch zip entry filename.");
		return false;
	}
	return noerr;
}

bool fsFetchZipEntryName(IFileSystem* pIO, uint64_t index, char* pBuffer, size_t* pSize, size_t bufferSize)
{
	ASSERT(pIO && pIO->pUser);
	// Both 0 or both not 0
	ASSERT(!pBuffer == !pSize);
	if (!isZipIO(pIO))
		return false;

	// make sure that zip file is opened
	if (!fsOpenZipFile(pIO))
	{
		LOGF(eERROR, "Failed to open zip file, while trying to fetch zip entry filename.");
		return false;
	}

	ZipFile* pZipFile = (ZipFile*)pIO->pUser;
	void* zip = pZipFile->pHandle;

	bool noerr = mz_zip_goto_entry(zip, index);
	mz_zip_file* pFileInfo = NULL;
	if (noerr)
		noerr = mz_zip_entry_get_info(zip, &pFileInfo);
	
	if (noerr && pFileInfo)
	{
		if (pBuffer)
		{
			size_t sizeToWrite = pFileInfo->filename_size < bufferSize ? pFileInfo->filename_size : bufferSize - 1;
			memcpy(pBuffer, pFileInfo->filename, sizeToWrite);
			pBuffer[pFileInfo->filename_size] = '\0';
		}
		*pSize = pFileInfo->filename_size;
	}



	if (!fsCloseZipFile(pIO))
	{
		LOGF(eERROR, "Failed to close zip file, while trying to fetch zip entry filename.");
		return false;
	}
	return noerr;
}

#else

#include "../Utilities/Interfaces/IFileSystem.h"

bool initZipFileSystem(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, const char* password, IFileSystem* pOut)
{
	return false;
}

bool exitZipFileSystem(IFileSystem* pZip)
{
	return false;
}

bool fsEntryCountZipFile(IFileSystem* pIO, uint64_t* pOut)
{
	return false;
}

bool fsOpenZipEntryByIndex(IFileSystem* pIO, uint64_t index, FileMode mode, const char* filePassword, FileStream* pOut)
{
	return false;
}

bool fsOpenZipFile(IFileSystem* pIO)
{
	return false;
}

bool fsCloseZipFile(IFileSystem* pIO)
{
	return false;
}

bool fsFetchZipEntryIndex(IFileSystem* pIO, ResourceDirectory resourceDir, const char* pFileName, uint64_t* pOut)
{
	return false;
}

bool fsFetchZipEntryName(IFileSystem* pIO, uint64_t index, char* pBuffer, size_t* pSize, size_t bufferSize)
{
	return false;
}

#endif
