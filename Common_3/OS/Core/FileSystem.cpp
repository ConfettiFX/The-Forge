/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

#ifdef __APPLE__
#include <unistd.h>
#include <limits.h>       // for UINT_MAX
#include <sys/stat.h>     // for mkdir
#include <sys/errno.h>    // for errno
#include <dirent.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#endif
#ifdef __linux__
#include <unistd.h>
#include <limits.h>       // for UINT_MAX
#include <sys/stat.h>     // for mkdir
#include <sys/errno.h>    // for errno
#include <sys/wait.h>
#include <dirent.h>
#endif

void translateFileAccessFlags(FileMode modeFlags, char* fileAccesString, int strLength)
{
	ASSERT(fileAccesString != NULL && strLength >= 4);
	memset(fileAccesString, '\0', strLength);
	int index = 0;

	// Read + Write uses w+ then filemode (b or t)
	if (modeFlags & FileMode::FM_Read && modeFlags & FileMode::FM_Write)
	{
		fileAccesString[index++] = 'w';
		fileAccesString[index++] = '+';
	}
	// Read + Append uses a+ then filemode (b or t)
	else if (modeFlags & FileMode::FM_Read && modeFlags & FileMode::FM_Append)
	{
		fileAccesString[index++] = 'a';
		fileAccesString[index++] = '+';
	}
	else
	{
		if (modeFlags & FileMode::FM_Read)
			fileAccesString[index++] = 'r';
		if (modeFlags & FileMode::FM_Write)
			fileAccesString[index++] = 'w';
		if (modeFlags & FileMode::FM_Append)
			fileAccesString[index++] = 'a';
	}

	if (modeFlags & FileMode::FM_Binary)
		fileAccesString[index++] = 'b';
	else
		fileAccesString[index++] = 't';

	fileAccesString[index++] = '\0';
}

//static const unsigned SKIP_BUFFER_SIZE = 1024;

extern const char* pszRoots[];

// pszBases handles behavior divergence between host device and remote device.
// Host device hosts assets in a relative path outside of the project root dir.
// Remove device, on the other hand, can only access files within project root dir.
// When we run on remote device, we ignore pszbase.
extern const char* pszBases[];
#if defined(__ANDROID__) || defined(_DURANGO) || defined(TARGET_IOS)
#define __IGNORE_PSZBASE 1
#else
#define __IGNORE_PSZBASE 0
#endif

static inline unsigned SDBMHash(unsigned hash, unsigned char c) { return c + (hash << 6) + (hash << 16) - hash; }

/************************************************************************/
// Deserializer implementation
/************************************************************************/
Deserializer::Deserializer(): mPosition(0), mSize(0) {}

Deserializer::Deserializer(unsigned size): mPosition(0), mSize(size) {}

Deserializer::~Deserializer() {}

unsigned Deserializer::GetChecksum() { return 0; }

int64_t Deserializer::ReadInt64()
{
	int64_t ret;
	Read(&ret, sizeof ret);
	return ret;
}

int Deserializer::ReadInt()
{
	int ret;
	Read(&ret, sizeof ret);
	return ret;
}

int16_t Deserializer::ReadShort()
{
	int16_t ret;
	Read(&ret, sizeof ret);
	return ret;
}

int8_t Deserializer::ReadByte()
{
	int8_t ret;
	Read(&ret, sizeof ret);
	return ret;
}

unsigned Deserializer::ReadUInt()
{
	unsigned ret;
	Read(&ret, sizeof ret);
	return ret;
}

uint16_t Deserializer::ReadUShort()
{
	uint16_t ret;
	Read(&ret, sizeof ret);
	return ret;
}

uint8_t Deserializer::ReadUByte()
{
	uint8_t ret;
	Read(&ret, sizeof ret);
	return ret;
}

bool Deserializer::ReadBool() { return ReadUByte() != 0; }

float Deserializer::ReadFloat()
{
	float ret;
	Read(&ret, sizeof ret);
	return ret;
}

double Deserializer::ReadDouble()
{
	double ret;
	Read(&ret, sizeof ret);
	return ret;
}

float2 Deserializer::ReadVector2()
{
	float data[2];
	Read(data, sizeof data);
	return float2(data[0], data[1]);
}

float3 Deserializer::ReadVector3()
{
	float data[3];
	Read(data, sizeof data);
	return float3(data[0], data[1], data[2]);
}

float3 Deserializer::ReadPackedVector3(float maxAbsCoord)
{
	float   invV = maxAbsCoord / 32767.0f;
	int16_t coords[3];
	Read(coords, sizeof coords);
	float3 ret(coords[0] * invV, coords[1] * invV, coords[2] * invV);
	return ret;
}

float4 Deserializer::ReadVector4()
{
	float data[4];
	Read(data, sizeof data);
	return float4(data[0], data[1], data[2], data[3]);
}

tinystl::string Deserializer::ReadString()
{
	tinystl::string ret;

	while (!IsEof())
	{
		char c = ReadByte();
		if (!c)
			break;
		else
			ret.push_back(c);
	}

	return ret;
}

tinystl::string Deserializer::ReadFileID()
{
	tinystl::string ret;
	ret.resize(4);
	Read(ret.begin(), 4);
	return ret;
}

tinystl::string Deserializer::ReadLine()
{
	tinystl::string ret;

	while (!IsEof())
	{
		char c = ReadByte();
		if (c == 10)
			break;
		if (c == 13)
		{
			// Peek next char to see if it's 10, and skip it too
			if (!IsEof())
			{
				char next = ReadByte();
				if (next != 10)
					Seek(mPosition - 1);
			}
			break;
		}

		ret.push_back(c);
	}

	return ret;
}
/************************************************************************/
// Serializer implementation
/************************************************************************/
/// Return length of a C string
static unsigned c_strlen(const char* str)
{
	if (!str)
		return 0;
#ifdef _MSC_VER
	return (unsigned)strlen(str);
#else
	const char* ptr = str;
	while (*ptr)
		++ptr;
	return (unsigned)(ptr - str);
#endif
}

Serializer::~Serializer() {}

bool Serializer::WriteInt64(int64_t value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteInt(int value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteShort(int16_t value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteByte(int8_t value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteUInt(unsigned value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteUShort(uint16_t value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteUByte(uint8_t value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteBool(bool value) { return WriteUByte((unsigned char)(value ? 1 : 0)) == 1; }

bool Serializer::WriteFloat(float value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteDouble(double value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteVector2(const float2& value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteVector3(const float3& value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WritePackedVector3(const float3& value, float maxAbsCoord)
{
	int16_t coords[3];
	float   v = 32767.0f / maxAbsCoord;

	coords[0] = (int16_t)(clamp(value.getX(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	coords[1] = (int16_t)(clamp(value.getY(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	coords[2] = (int16_t)(clamp(value.getZ(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	return Write(&coords[0], sizeof coords) == sizeof coords;
}

bool Serializer::WriteVector4(const float4& value) { return Write(&value, sizeof value) == sizeof value; }

bool Serializer::WriteString(const tinystl::string& value)
{
	const char* chars = value.c_str();
	// Count length to the first zero, because ReadString() does the same
	unsigned length = c_strlen(chars);
	return Write(chars, length) == length + 1;
}

bool Serializer::WriteFileID(const tinystl::string& value)
{
	bool     success = true;
	unsigned length = (unsigned)min((int)(uint32_t)value.size(), 4);

	success &= Write(value.c_str(), length) == length;
	for (unsigned i = (uint32_t)value.size(); i < 4; ++i)
		success &= WriteByte(' ');
	return success;
}

bool Serializer::WriteLine(const tinystl::string& value)
{
	bool success = true;
	success &= Write(value.c_str(), (uint32_t)value.size()) == (uint32_t)value.size();
	// Only write '\n'; if we are writing to a text file it should be opened in text mode.
	// Platforms other than Windows won't interpret the '\r' correctly if we add it here.
	success &= WriteUByte(10);
	return success;
}
/************************************************************************/
// File implementation
/************************************************************************/
File::File(): mMode(FileMode::FM_Read), pHandle(0), mOffset(0), mChecksum(0), mReadSyncNeeded(false), mWriteSyncNeeded(false) {}

bool File::Open(const tinystl::string& _fileName, FileMode mode, FSRoot root)
{
	tinystl::string fileName = FileSystem::FixPath(_fileName, root);

	Close();

	if (fileName.size() == 0)
	{
		LOGERRORF("Could not open file with empty name");
		return false;
	}

	char fileAcessStr[8];
	translateFileAccessFlags(mode, fileAcessStr, sizeof(fileAcessStr));
	pHandle = open_file(fileName, fileAcessStr);

	if (!pHandle)
	{
		LOGERRORF("Could not open file %s", fileName.c_str());
		return false;
	}

	mFileName = fileName;
	mMode = mode;
	mPosition = 0;
	mOffset = 0;
	mChecksum = 0;
	mReadSyncNeeded = false;
	mWriteSyncNeeded = false;

	size_t size = FileSystem::GetFileSize(pHandle);
	if (size > UINT_MAX)
	{
		LOGERRORF("Could not open file %s which is larger than 4GB", fileName.c_str());
		Close();
		mSize = 0;
		return false;
	}

	mSize = (unsigned)size;
	return true;
}

bool File::Close()
{
	bool ret = false;
	if (pHandle)
	{
		ret = close_file(pHandle);
		pHandle = 0;
		mPosition = 0;
		mSize = 0;
		mOffset = 0;
		mChecksum = 0;
	}
	return ret;
}

void File::Flush()
{
	if (pHandle)
		flush_file(pHandle);
}

unsigned File::Read(void* dest, unsigned size)
{
	if (!pHandle)
	{
		// Avoid spamming stderr
		return 0;
	}

	if (IsWriteOnly())
	{
		LOGERROR("File not opened for reading");
		return 0;
	}

	if (size + mPosition > mSize)
		size = mSize - mPosition;
	if (!size)
		return 0;

	if (mReadSyncNeeded)
	{
		seek_file(pHandle, mPosition + mOffset, SEEK_SET);
		mReadSyncNeeded = false;
	}

	size = (unsigned int)read_file(dest, size, pHandle);
	mWriteSyncNeeded = true;
	mPosition += size;
	return size;
}

unsigned File::Seek(unsigned position, SeekDir seekDir /* = SeekDir::SEEK_DIR_BEGIN*/)
{
	if (!pHandle)
	{
		// Avoid spamming stderr
		return 0;
	}

	//If reading or appending don't seek past the end
	if ((mMode & FileMode::FM_Read || mMode & FileMode::FM_Append) && position > mSize)
		position = mSize;

	int origin = -1;
	switch (seekDir)
	{
		case SEEK_DIR_BEGIN: origin = SEEK_SET; break;
		case SEEK_DIR_CUR: origin = SEEK_CUR; break;
		case SEEK_DIR_END: origin = SEEK_END; break;
		default: break;
	}
	seek_file(pHandle, position + mOffset, origin);
	mPosition = position;
	mReadSyncNeeded = false;
	mWriteSyncNeeded = false;
	return mPosition;
}

unsigned File::Write(const void* data, unsigned size)
{
	if (!pHandle)
	{
		// Allow sparse seeks if writing
		return 0;
	}

	if (IsReadOnly())
	{
		LOGERROR("File not opened for writing");
		return 0;
	}

	if (!size)
		return 0;

	if (mWriteSyncNeeded)
	{
		seek_file(pHandle, mPosition + mOffset, SEEK_SET);
		mWriteSyncNeeded = false;
	}

	// fwrite returns how many bytes were written.
	// which should be the same as size.
	// If not, then it's a write error.
	if (write_file(data, size, pHandle) != size)
	{
		// Return to the position where the write began
		seek_file(pHandle, mPosition + mOffset, SEEK_SET);
		LOGERROR("Error while writing to file " + GetName());
		return 0;
	}

	mReadSyncNeeded = true;
	mPosition += size;
	if (mPosition > mSize)
		mSize = mPosition;

	return size;
}

unsigned File::GetChecksum()
{
	if (mOffset || mChecksum)
		return mChecksum;

	if (!pHandle || IsWriteOnly())
		return 0;

	unsigned oldPos = mPosition;
	mChecksum = 0;

	Seek(0);
	while (!IsEof())
	{
		unsigned char block[1024];
		unsigned      readBytes = Read(block, 1024);
		for (unsigned i = 0; i < readBytes; ++i)
			mChecksum = SDBMHash(mChecksum, block[i]);
	}

	Seek(oldPos);
	return mChecksum;
}

tinystl::string File::ReadText()
{
	Seek(0);
	tinystl::string text;

	if (!mSize)
		return tinystl::string();

	text.resize(mSize);

	Read((void*)text.c_str(), mSize);

	return text;
}

MemoryBuffer::MemoryBuffer(const void* data, unsigned size): Deserializer(size), pBuffer((unsigned char*)data), mReadOnly(true)
{
	if (!pBuffer)
		mSize = 0;
}

MemoryBuffer::MemoryBuffer(void* data, unsigned size): Deserializer(size), pBuffer((unsigned char*)data), mReadOnly(false)
{
	if (!pBuffer)
		mSize = 0;
}

unsigned MemoryBuffer::Read(void* dest, unsigned size)
{
	if (size + mPosition > mSize)
		size = mSize - mPosition;
	if (!size)
		return 0;

	unsigned char* srcPtr = &pBuffer[mPosition];
	unsigned char* destPtr = (unsigned char*)dest;
	mPosition += size;

	unsigned copySize = size;
	while (copySize >= sizeof(unsigned))
	{
		*((unsigned*)destPtr) = *((unsigned*)srcPtr);
		srcPtr += sizeof(unsigned);
		destPtr += sizeof(unsigned);
		copySize -= sizeof(unsigned);
	}
	if (copySize & sizeof(uint16_t))
	{
		*((uint16_t*)destPtr) = *((uint16_t*)srcPtr);
		srcPtr += sizeof(uint16_t);
		destPtr += sizeof(uint16_t);
	}
	if (copySize & 1)
		*destPtr = *srcPtr;

	return size;
}

unsigned MemoryBuffer::Seek(unsigned position, SeekDir seekDir /* = SeekDir::SEEK_DIR_BEGIN*/)
{
	UNREF_PARAM(seekDir);
	if (position > mSize)
		position = mSize;

	mPosition = position;
	return mPosition;
}

unsigned MemoryBuffer::Write(const void* data, unsigned size)
{
	if (size + mPosition > mSize)
		size = mSize - mPosition;
	if (!size)
		return 0;

	unsigned char* srcPtr = (unsigned char*)data;
	unsigned char* destPtr = &pBuffer[mPosition];
	mPosition += size;

	unsigned copySize = size;
	while (copySize >= sizeof(unsigned))
	{
		*((unsigned*)destPtr) = *((unsigned*)srcPtr);
		srcPtr += sizeof(unsigned);
		destPtr += sizeof(unsigned);
		copySize -= sizeof(unsigned);
	}
	if (copySize & sizeof(uint16_t))
	{
		*((uint16_t*)destPtr) = *((uint16_t*)srcPtr);
		srcPtr += sizeof(uint16_t);
		destPtr += sizeof(uint16_t);
	}
	if (copySize & 1)
		*destPtr = *srcPtr;

	return size;
}
/************************************************************************/
/************************************************************************/
tinystl::string FileSystem::mModifiedRootPaths[FSRoot::FSR_Count] = { "" };
tinystl::string FileSystem::mProgramDir = "";

void FileSystem::SetRootPath(FSRoot root, const tinystl::string& rootPath)
{
	ASSERT(root < FSR_Count);
	mModifiedRootPaths[root] = rootPath;
}

void FileSystem::ClearModifiedRootPaths()
{
	for (tinystl::string& s : mModifiedRootPaths)
		s = "";
}

unsigned FileSystem::GetLastModifiedTime(const tinystl::string& fileName) { return (unsigned)get_file_last_modified_time(fileName); }

unsigned FileSystem::GetFileSize(FileHandle handle)
{
	long curPos = tell_file((::FILE*)handle);
	seek_file(handle, 0, SEEK_END);
	size_t length = tell_file((::FILE*)handle);
	seek_file((::FILE*)handle, curPos, SEEK_SET);
	return (unsigned)length;
}

bool FileSystem::FileExists(const tinystl::string& _fileName, FSRoot _root)
{
	tinystl::string fileName = FileSystem::FixPath(_fileName, _root);
#ifdef _DURANGO
	return (fopen(fileName, "rb") != NULL);
#else
	return ((access(fileName.c_str(), 0)) != -1);
#endif
}

// TODO: FIX THIS FUNCTION
tinystl::string FileSystem::FixPath(const tinystl::string& pszFileName, FSRoot root)
{
	ASSERT(root < FSR_Count);
	tinystl::string res;
	if (root != FSR_Absolute && pszFileName[1U] != ':' &&
		pszFileName[0U] != '/')    //Quick hack to ignore root changes when a absolute path is given in windows or GNU
	{
		// was the path modified? if so use that, otherwise use static array
		if (mModifiedRootPaths[root].size() != 0)
			res = mModifiedRootPaths[root] + pszFileName;
		else
			res = tinystl::string(pszRoots[root]) + pszFileName;
#if !__IGNORE_PSZBASE
		res = tinystl::string(pszBases[root]) + res;
#endif
	}
	else
	{
		res = pszFileName;
	}

#ifdef TARGET_IOS
	// Dont append bundle path if input path is already an absolute path
	// Example: Files outside the application folder picked using the Files API, iCloud files, ...
	if (!absolute_path(res))
	{
		// iOS is deployed on the device so we need to get the
		// bundle path via get_current_dir()
		const tinystl::string currDir = get_current_dir();
		if (res.find(currDir, 0) == tinystl::string::npos)
			res = currDir + "/" + res;

		res = GetInternalPath(res);    // eliminate windows separators here.
	}
#endif

	/*
	ASSERT( root < FSR_Count );
	// absolute or already relative to absolute paths we can assume are already fixed,
	// of course deteminting whats absolute is platform specific
	// so this will likely require work for some platforms
	const tinystl::string filename(pszFileName);
	// on all but unix filesystem : generally mean volume relative so absolute
	if( filename.find( ":" ) ){
	return filename;
	}
	// if it has relative movement, if must know where it belongs.
	if( filename.find( ".." ) ) {
	return filename;
	}
	// TODO unix root, google Filesytem has a good open source implementation
	// that should work on most platforms.

	tinystl::string res = tinystl::string (rootPaths[root] ) + filename;

	#ifdef	SN_TARGET_PS3
	res.replace("\\","/");
	//	Igor: this can be handled differently: just replace the last 3 chars.
	if (root==FSR_Textures)
	res.replace(".dds",".gtf");
	#endif
	*/
	return res;
}

tinystl::string FileSystem::GetRootPath(FSRoot root)
{
	if (mModifiedRootPaths[root].size())
		return mModifiedRootPaths[root];
	else
		return pszRoots[root];
}

void FileSystem::SplitPath(
	const tinystl::string& fullPath, tinystl::string* pathName, tinystl::string* fileName, tinystl::string* extension,
	bool lowercaseExtension)
{
	tinystl::string fullPathCopy = GetInternalPath(fullPath);

	unsigned extPos = fullPathCopy.find_last('.');
	unsigned pathPos = fullPathCopy.find_last('/');

	if (extPos != tinystl::string::npos && (pathPos == tinystl::string::npos || extPos > pathPos))
	{
		*extension = fullPathCopy.substring(extPos);
		if (lowercaseExtension)
			*extension = extension->to_lower();
		fullPathCopy = fullPathCopy.substring(0, extPos);
	}
	else
		extension->resize(0);

	pathPos = fullPathCopy.find_last('/');
	if (pathPos != tinystl::string::npos)
	{
		*fileName = fullPathCopy.substring(pathPos + 1);
		*pathName = fullPathCopy.substring(0, pathPos + 1);
	}
	else
	{
		*fileName = fullPathCopy;
		pathName->resize(0);
	}
}

tinystl::string FileSystem::GetPath(const tinystl::string& fullPath)
{
	tinystl::string path, file, extension;
	SplitPath(fullPath, &path, &file, &extension);
	return path;
}

tinystl::string FileSystem::GetFileName(const tinystl::string& fullPath)
{
	tinystl::string path, file, extension;
	SplitPath(fullPath, &path, &file, &extension);
	return file;
}

tinystl::string FileSystem::GetExtension(const tinystl::string& fullPath, bool lowercaseExtension)
{
	tinystl::string path, file, extension;
	SplitPath(fullPath, &path, &file, &extension, lowercaseExtension);
	return extension;
}

tinystl::string FileSystem::GetFileNameAndExtension(const tinystl::string& fileName, bool lowercaseExtension)
{
	tinystl::string path, file, extension;
	SplitPath(fileName, &path, &file, &extension, lowercaseExtension);
	return file + extension;
}

tinystl::string FileSystem::ReplaceExtension(const tinystl::string& fullPath, const tinystl::string& newExtension)
{
	tinystl::string path, file, extension;
	SplitPath(fullPath, &path, &file, &extension);
	return path + file + newExtension;
}

tinystl::string FileSystem::AddTrailingSlash(const tinystl::string& pathName)
{
	tinystl::string ret = pathName.trimmed();
	ret.replace('\\', '/');
	if (ret.size() != 0 && ret.at((uint32_t)ret.size() - 1) != '/')
		ret.push_back('/');
	return ret;
}

tinystl::string FileSystem::RemoveTrailingSlash(const tinystl::string& pathName)
{
	tinystl::string ret = pathName.trimmed();
	ret.replace('\\', '/');
	if (ret.size() != 0 && ret.at((uint32_t)ret.size() - 1) == '/')
		ret.resize((uint32_t)ret.size() - 1);
	return ret;
}

tinystl::string FileSystem::GetParentPath(const tinystl::string& path)
{
	unsigned pos = RemoveTrailingSlash(path).find_last('/');
	if (pos != tinystl::string::npos)
		return path.substring(0, pos + 1);
	else
		return tinystl::string();
}

tinystl::string FileSystem::GetInternalPath(const tinystl::string& pathName)
{
	tinystl::string ret = pathName;
	ret.replace('\\', '/');
	return ret;
}

tinystl::string FileSystem::GetNativePath(const tinystl::string& pathName)
{
#ifdef _WIN32
	return pathName.replaced('/', '\\');
#else
	return pathName;
#endif
}

bool FileSystem::CopyFile(const tinystl::string& src, const tinystl::string& dst, bool failIfExists)
{
	if (failIfExists && FileExists(dst, FSR_Absolute))
		return false;

	return copy_file(src, dst);
}

bool FileSystem::DirExists(const tinystl::string& pathName)
{
#ifndef _WIN32
	// Always return true for the root directory
	if (pathName == "/")
		return true;
#endif

	tinystl::string fixedName = GetNativePath(RemoveTrailingSlash(pathName));

#ifdef _WIN32
	DWORD attributes = GetFileAttributesA(fixedName.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
		return false;
#else
	struct stat st;
	if (stat(fixedName.c_str(), &st) || !(st.st_mode & S_IFDIR))
		return false;
#endif

	return true;
}

bool FileSystem::CreateDir(const tinystl::string& pathName)
{
	// Create each of the parents if necessary
	tinystl::string parentPath = GetParentPath(pathName);
	if ((uint32_t)parentPath.size() > 1 && !DirExists(parentPath))
	{
		if (!CreateDir(parentPath))
			return false;
	}

#ifdef _WIN32
	bool success = (CreateDirectoryA(RemoveTrailingSlash(pathName).c_str(), NULL) == TRUE) || (GetLastError() == ERROR_ALREADY_EXISTS);
#else
	bool success = mkdir(GetNativePath(RemoveTrailingSlash(pathName)).c_str(), S_IRWXU) == 0 || errno == EEXIST;
#endif

	if (success)
		LOGDEBUG("Created directory " + pathName);
	else
		LOGERROR("Failed to create directory " + pathName);

	return success;
}

int FileSystem::SystemRun(const tinystl::string& fileName, const tinystl::vector<tinystl::string>& arguments, tinystl::string stdOutFile)
{
	UNREF_PARAM(arguments);
	tinystl::string fixedFileName = GetNativePath(fileName);

#ifdef _DURANGO
	ASSERT(!"UNIMPLEMENTED");
	return -1;

#elif defined(_WIN32)
	// Add .exe extension if no extension defined
	if (GetExtension(fixedFileName).size() == 0)
		fixedFileName += ".exe";

	tinystl::string commandLine = "\"" + fixedFileName + "\"";
	for (unsigned i = 0; i < (unsigned)arguments.size(); ++i)
		commandLine += " " + arguments[i];

	HANDLE stdOut = NULL;
	if (stdOutFile != "")
	{
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		stdOut = CreateFileA(stdOutFile, GENERIC_ALL, FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	STARTUPINFOA        startupInfo;
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo, 0, sizeof startupInfo);
	memset(&processInfo, 0, sizeof processInfo);
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.hStdOutput = stdOut;
	startupInfo.hStdError = stdOut;

	if (!CreateProcessA(
			NULL, (LPSTR)commandLine.c_str(), NULL, NULL, stdOut ? TRUE : FALSE, CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo))
		return -1;

	WaitForSingleObject(processInfo.hProcess, INFINITE);
	DWORD exitCode;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	if (stdOut)
	{
		CloseHandle(stdOut);
	}

	return exitCode;
#elif defined(__linux__)
	tinystl::vector<const char*> argPtrs;
	tinystl::string              cmd(fixedFileName.c_str());
	char                         space = ' ';
	cmd.append(&space, &space + 1);
	for (unsigned i = 0; i < (unsigned)arguments.size(); ++i)
	{
		cmd.append(arguments[i].begin(), arguments[i].end());
	}

	int res = system(cmd.c_str());
	return res;
#else
	pid_t pid = fork();
	if (!pid)
	{
		tinystl::vector<const char*> argPtrs;
		argPtrs.push_back(fixedFileName.c_str());
		for (unsigned i = 0; i < (unsigned)arguments.size(); ++i)
			argPtrs.push_back(arguments[i].c_str());
		argPtrs.push_back(NULL);

		execvp(argPtrs[0], (char**)&argPtrs[0]);
		return -1;    // Return -1 if we could not spawn the process
	}
	else if (pid > 0)
	{
		int exitCode = EINTR;
		while (exitCode == EINTR)
			wait(&exitCode);
		return exitCode;
	}
	else
		return -1;
#endif
}

bool FileSystem::Delete(const tinystl::string& fileName)
{
#ifdef _WIN32
	return DeleteFileA(GetNativePath(fileName).c_str()) != 0;
#else
	return remove(GetNativePath(fileName).c_str()) == 0;
#endif
}
