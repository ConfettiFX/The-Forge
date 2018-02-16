/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
#include "../Math/FloatUtil.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"

#ifdef __APPLE__
#include <unistd.h>
#include <limits.h>  // for UINT_MAX
#include <sys/stat.h>  // for mkdir
#include <sys/errno.h> // for errno
#endif
#ifdef _WIN32
#include  <io.h>
#include  <stdio.h>
#include  <stdlib.h>
#endif

static const char* pszFileAccessFlags[] =
{
	"rb",	//!< 	FM_ReadBinary		= 0,
	"wb",	//!< 	FM_WriteBinary,
	"w+b",	//!< 	FM_ReadWriteBinary,
	"r",	//!< 	FM_Read,
	"w",	//!< 	FM_Write,
	"w+",	//!< 	FM_ReadWrite,
	"--",   //!<	FM_Count
};

//static const unsigned SKIP_BUFFER_SIZE = 1024;

extern const char* pszRoots[];

static inline unsigned SDBMHash(unsigned hash, unsigned char c) { return c + (hash << 6) + (hash << 16) - hash; }

/************************************************************************/
// Deserializer implementation
/************************************************************************/
Deserializer::Deserializer() :
	mPosition(0),
	mSize(0)
{
}

Deserializer::Deserializer(unsigned size) :
	mPosition(0),
	mSize(size)
{
}

Deserializer::~Deserializer()
{
}

unsigned Deserializer::GetChecksum()
{
	return 0;
}

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

bool Deserializer::ReadBool()
{
	return ReadUByte() != 0;
}

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
	float invV = maxAbsCoord / 32767.0f;
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

String Deserializer::ReadString()
{
	String ret;

	while (!IsEof())
	{
		char c = ReadByte();
		if (!c)
			break;
		else
			ret.push_back (c);
	}

	return ret;
}

String Deserializer::ReadFileID()
{
	String ret;
	ret.resize(4);
	Read(&ret[0U], 4);
	return ret;
}

String Deserializer::ReadLine()
{
	String ret;

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

Serializer::~Serializer()
{
}

bool Serializer::WriteInt64(int64_t value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteInt(int value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteShort(int16_t value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteByte(int8_t value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteUInt(unsigned value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteUShort(uint16_t value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteUByte(uint8_t value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteBool(bool value)
{
	return WriteUByte((unsigned char)(value ? 1 : 0)) == 1;
}

bool Serializer::WriteFloat(float value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteDouble(double value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteVector2(const float2& value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteVector3(const float3& value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WritePackedVector3(const float3& value, float maxAbsCoord)
{
	int16_t coords[3];
	float v = 32767.0f / maxAbsCoord;

	coords[0] = (int16_t)(clamp(value.getX(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	coords[1] = (int16_t)(clamp(value.getY(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	coords[2] = (int16_t)(clamp(value.getZ(), -maxAbsCoord, maxAbsCoord) * v + 0.5f);
	return Write(&coords[0], sizeof coords) == sizeof coords;
}

bool Serializer::WriteVector4(const float4& value)
{
	return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteString(const String& value)
{
	const char* chars = value.c_str();
	// Count length to the first zero, because ReadString() does the same
	unsigned length = c_strlen(chars);
	return Write(chars, length) == length + 1;
}

bool Serializer::WriteFileID(const String& value)
{
	bool success = true;
	unsigned length = (unsigned)min((int)value.getLength(), 4);

	success &= Write(value.c_str(), length) == length;
	for (unsigned i = value.getLength(); i < 4; ++i)
		success &= WriteByte(' ');
	return success;
}

bool Serializer::WriteLine(const String& value)
{
	bool success = true;
	success &= Write(value.c_str(), value.getLength()) == value.getLength();
	// Only write '\n'; if we are writing to a text file it should be opened in text mode.
	// Platforms other than Windows won't interpret the '\r' correctly if we add it here.
	success &= WriteUByte(10);
	return success;
}
/************************************************************************/
// File implementation
/************************************************************************/
File::File() :
	mMode(FileMode::FM_Read),
	pHandle(0),
	mOffset(0),
	mChecksum(0),
	mReadSyncNeeded(false),
	mWriteSyncNeeded(false)
{
}

bool File::Open(const String& _fileName, FileMode mode, FSRoot root)
{
	String fileName = FileSystem::FixPath(_fileName, root);

	Close();

	if (fileName.isEmpty())
	{
		LOGERRORF("Could not open file with empty name");
		return false;
	}

	pHandle = _openFile(fileName, pszFileAccessFlags[mode]);

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

void File::Close()
{
	if (pHandle)
	{
		_closeFile(pHandle);
		pHandle = 0;
		mPosition = 0;
		mSize = 0;
		mOffset = 0;
		mChecksum = 0;
	}
}

void File::Flush()
{
	if (pHandle)
		_flushFile (pHandle);
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
		_seekFile(pHandle, mPosition + mOffset, SEEK_SET);
		mReadSyncNeeded = false;
	}

	_readFile(dest, size, pHandle);
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

	if (mMode == FileMode::FM_Read && position > mSize)
		position = mSize;

	int origin = -1;
	switch (seekDir)
	{
	case SEEK_DIR_BEGIN:
		origin = SEEK_SET;
		break;
	case SEEK_DIR_CUR:
		origin = SEEK_CUR;
		break;
	case SEEK_DIR_END:
		origin = SEEK_END;
		break;
	default:
		break;
	}
	_seekFile(pHandle, position + mOffset, origin);
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

	if (IsReadOnly ())
	{
		LOGERROR("File not opened for writing");
		return 0;
	}

	if (!size)
		return 0;

	if (mWriteSyncNeeded)
	{
		_seekFile(pHandle, mPosition + mOffset, SEEK_SET);
		mWriteSyncNeeded = false;
	}

	if (_writeFile(data, size, pHandle) != 1)
	{
		// Return to the position where the write began
		_seekFile(pHandle, mPosition + mOffset, SEEK_SET);
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
		unsigned readBytes = Read(block, 1024);
		for (unsigned i = 0; i < readBytes; ++i)
			mChecksum = SDBMHash(mChecksum, block[i]);
	}

	Seek(oldPos);
	return mChecksum;
}

String File::ReadText()
{
	Seek(0);
	String text;

	if (!mSize)
		return String ();

	text.resize(mSize);

	Read((void*)text.c_str(), mSize);

	return text;
}

MemoryBuffer::MemoryBuffer(const void* data, unsigned size) :
	Deserializer(size),
	pBuffer((unsigned char*)data),
	mReadOnly(true)
{
	if (!pBuffer)
		mSize = 0;
}

MemoryBuffer::MemoryBuffer(void* data, unsigned size) :
	Deserializer(size),
	pBuffer((unsigned char*)data),
	mReadOnly(false)
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

String FileSystem::mModifiedRootPaths[FSRoot::FSR_Count] = { "" };
String FileSystem::mProgramDir = "";

void FileSystem::SetRootPath(FSRoot root, const String& rootPath)
{
	ASSERT(root < FSR_Count);
	mModifiedRootPaths[root] = rootPath;
}

void FileSystem::ClearModifiedRootPaths()
{
	for (String& s : mModifiedRootPaths)
		s = "";
}

unsigned FileSystem::GetLastModifiedTime(const String& fileName)
{
	return (unsigned)_getFileLastModifiedTime(fileName);
}

unsigned FileSystem::GetFileSize(FileHandle handle)
{
	long curPos = _tellFile((::FILE*)handle);
	_seekFile(handle, 0, SEEK_END);
	size_t length = _tellFile((::FILE*)handle);
	_seekFile((::FILE*)handle, curPos, SEEK_SET);
	return (unsigned)length;
}
    
bool FileSystem::FileExists(const String& _fileName, FSRoot _root)
{
	String fileName = FileSystem::FixPath(_fileName, _root);
#ifdef _DURANGO
	return (fopen(fileName, "rb") != NULL);
#else
    return ((access(fileName.c_str(), 0 )) != -1);
#endif
}

// TODO: FIX THIS FUNCTION
String FileSystem::FixPath(const String& pszFileName, FSRoot root)
{
	if (root == FSR_Absolute)
		return pszFileName;

	ASSERT(root < FSR_Count);
	String res;
	if (pszFileName[1U] != ':' && pszFileName[0U] != '/') //Quick hack to ignore root changes when a absolute path is given in windows or GNU
	{
		// was the path modified? if so use that, otherwise use static array
		if (!mModifiedRootPaths[root].isEmpty())
			res = mModifiedRootPaths[root] + pszFileName;
		else
			res = String(pszRoots[root]) + pszFileName;
	}
	else
	{
		res = pszFileName;
	}
	/*
	ASSERT( root < FSR_Count );
	// absolute or already relative to absolute paths we can assume are already fixed,
	// of course deteminting whats absolute is platform specific
	// so this will likely require work for some platforms
	const String filename(pszFileName);
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

	String res = String (rootPaths[root] ) + filename;

	#ifdef	SN_TARGET_PS3
	res.replace("\\","/");
	//	Igor: this can be handled differently: just replace the last 3 chars.
	if (root==FSR_Textures)
	res.replace(".dds",".gtf");
	#endif
	*/
	return res;
}

void FileSystem::SplitPath(const String& fullPath, String* pathName, String* fileName, String* extension, bool lowercaseExtension)
{
	String fullPathCopy = GetInternalPath(fullPath);

	unsigned extPos = fullPathCopy.find_last('.');
	unsigned pathPos = fullPathCopy.find_last('/');

	if (extPos != String::npos && (pathPos == String::npos || extPos > pathPos))
	{
		*extension = fullPathCopy.substring(extPos);
		if (lowercaseExtension)
			*extension = extension->to_lower();
		fullPathCopy = fullPathCopy.substring(0, extPos);
	}
	else
		extension->resize(0);

	pathPos = fullPathCopy.find_last('/');
	if (pathPos != String::npos)
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

String FileSystem::GetPath(const String& fullPath)
{
	String path, file, extension;
	SplitPath(fullPath, &path, &file, &extension);
	return path;
}

String FileSystem::GetFileName(const String& fullPath)
{
	String path, file, extension;
	SplitPath(fullPath, &path, &file, &extension);
	return file;
}

String FileSystem::GetExtension(const String& fullPath, bool lowercaseExtension)
{
	String path, file, extension;
	SplitPath(fullPath, &path, &file, &extension, lowercaseExtension);
	return extension;
}

String FileSystem::GetFileNameAndExtension(const String& fileName, bool lowercaseExtension)
{
	String path, file, extension;
	SplitPath(fileName, &path, &file, &extension, lowercaseExtension);
	return file + extension;
}

String FileSystem::ReplaceExtension(const String& fullPath, const String& newExtension)
{
	String path, file, extension;
	SplitPath(fullPath, &path, &file, &extension);
	return path + file + newExtension;
}

String FileSystem::AddTrailingSlash(const String& pathName)
{
	String ret = pathName.trimmed();
	ret.replace('\\', '/');
	if (!ret.isEmpty() && ret.at(ret.getLength() - 1) != '/')
		ret.push_back('/');
	return ret;
}

String FileSystem::RemoveTrailingSlash(const String& pathName)
{
	String ret = pathName.trimmed();
	ret.replace('\\', '/');
	if (!ret.isEmpty() && ret.at(ret.getLength() - 1) == '/')
		ret.resize(ret.getLength() - 1);
	return ret;
}

String FileSystem::GetParentPath(const String& path)
{
	unsigned pos = RemoveTrailingSlash(path).find_last('/');
	if (pos != String::npos)
		return path.substring(0, pos + 1);
	else
		return String();
}

String FileSystem::GetInternalPath(const String& pathName)
{
	String ret = pathName;
	ret.replace('\\', '/');
	return ret;
}

String FileSystem::GetNativePath(const String& pathName)
{
#ifdef _WIN32
	return pathName.replaced('/', '\\');
#else
	return pathName;
#endif
}

bool FileSystem::DirExists(const String& pathName)
{
#ifndef _WIN32
	// Always return true for the root directory
	if (pathName == "/")
		return true;
#endif

	String fixedName = GetNativePath(RemoveTrailingSlash(pathName));

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

bool FileSystem::CreateDir(const String& pathName)
{
	// Create each of the parents if necessary
	String parentPath = GetParentPath(pathName);
	if (parentPath.getLength() > 1 && !DirExists(parentPath))
	{
		if (!CreateDir(parentPath))
			return false;
	}

#ifdef _WIN32
	bool success = (CreateDirectoryA(RemoveTrailingSlash(pathName).c_str(), nullptr) == TRUE) ||
		(GetLastError() == ERROR_ALREADY_EXISTS);
#else
	bool success = mkdir(GetNativePath(RemoveTrailingSlash(pathName)).c_str(), S_IRWXU) == 0 || errno == EEXIST;
#endif

	if (success)
		LOGDEBUG("Created directory " + pathName);
	else
		LOGERROR("Failed to create directory " + pathName);

	return success;
}

int FileSystem::SystemRun(const String& fileName, const tinystl::vector<String>& arguments, String stdOutFile)
{
  UNREF_PARAM(arguments);
	String fixedFileName = GetNativePath(fileName);

#ifdef _DURANGO
	ASSERT(!"UNIMPLEMENTED");
	return -1;

#elif defined(_WIN32)
	// Add .exe extension if no extension defined
	if (GetExtension(fixedFileName).isEmpty())
		fixedFileName += ".exe";

	String commandLine = "\"" + fixedFileName + "\"";
	for (unsigned i = 0; i < (unsigned)arguments.size(); ++i)
		commandLine += " " + arguments[i];

	HANDLE stdOut = NULL;
	if (stdOutFile != "")
	{
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		stdOut = CreateFileA(stdOutFile,
			GENERIC_ALL,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			&sa,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	}

	STARTUPINFOA startupInfo;
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo, 0, sizeof startupInfo);
	memset(&processInfo, 0, sizeof processInfo);
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.hStdOutput = stdOut;
	startupInfo.hStdError = stdOut;

	if (!CreateProcessA(nullptr, (LPSTR)commandLine.c_str(), nullptr, nullptr, stdOut ? TRUE : FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo))
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
		return -1; // Return -1 if we could not spawn the process
	}
	else if (pid > 0)
	{
		int exitCode = EINTR;
        while(exitCode == EINTR) wait(&exitCode);
		return exitCode;
	}
	else
		return -1;
#endif
}

bool FileSystem::Delete(const String& fileName)
{
#ifdef _WIN32
	return DeleteFileA(GetNativePath(fileName).c_str()) != 0;
#else
	return remove(GetNativePath(fileName).c_str()) == 0;
#endif
}
