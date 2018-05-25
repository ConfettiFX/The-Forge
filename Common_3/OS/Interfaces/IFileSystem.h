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
#pragma once

//Use Virtual FileSystem
//#define USE_VFS

#include "../Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

typedef void* FileHandle;
/************************************************************************************************
* If you change this class please check the following projects to ensure you don't break them:  *
* 1) BlackFootBlade																				*
* 2) Aura																						*
* 3) Gladiator																					*
* Also ask yourself if all of those projects will need the change, or just the one you're work- *
* ing on. If it is just the one you're working on please add it to an inherited version of the  *
* class within your project.																	*
************************************************************************************************/

/// Low level file system interface providing basic file I/O operations
/// Implementations platform dependent
FileHandle _openFile(const char* filename, const char* flags);
void _closeFile(FileHandle handle);
void _flushFile(FileHandle handle);
size_t _readFile(void *buffer, size_t byteCount, FileHandle handle);
bool _seekFile(FileHandle handle, long offset, int origin);
long _tellFile(FileHandle handle);
size_t _writeFile(const void *buffer, size_t byteCount, FileHandle handle);
size_t _getFileLastModifiedTime(const char* _fileName);

String _getCurrentDir();
String _getExePath();
String _getAppPrefsDir(const char* org, const char* app);
String _getUserDocumentsDir();

void _setCurrentDir(const char* path);

enum FileMode
{
	FM_ReadBinary = 0,
	FM_WriteBinary,
	FM_ReadWriteBinary,
	FM_Read,
	FM_Write,
	FM_ReadWrite,
	FM_Count
};

// TODO : File System should be reworked so that we don't need all of these vague enums.
//      One suggesting is to just have a list of directories to check for each resource type.
//      Applications would then just need to add the paths they need to that list.
enum FSRoot {
	// universal binary shader place
	FSR_BinShaders = 0,
	// the main applications shader source directory
	FSR_SrcShaders,
        
    // shared shaders binary directory
    FSR_BinShaders_Common,
    // shared shaders source directory
    FSR_SrcShaders_Common,

	// the main application texture source directory (TODO processed texture folder)
	FSR_Textures,

	FSR_Meshes, // Meshes
	FSR_Builtin_Fonts,
	FSR_GpuConfig,
	FSR_OtherFiles,

	// libraries will want there own directories
	// support for upto 100 libraries
	____fsr_lib_counter_begin = FSR_OtherFiles,

	// Add libraries here
	FSR_Lib0_SrcShaders,
	FSR_Lib0_Texture,
	FSR_Lib0_OtherFiles,

	FSR_Lib1_OtherFiles,

	FSR_Lib2_OtherFiles,

	FSR_Lib3_OtherFiles,

	FSR_Lib4_OtherFiles,

	FSR_Lib5_OtherFiles,

	FSR_Lib6_OtherFiles,

	FSR_Lib7_OtherFiles,

	FSR_Lib8_OtherFiles,

	FSR_Lib9_OtherFiles,

	FSR_Lib10_OtherFiles,

	FSR_Lib11_OtherFiles,

	FSR_Lib12_OtherFiles,

	// FSR_Lib1_SrcShaders,
	// FSR_Lib1_Texture,
	// FSR_Lib1_OtherFiles,
	// and etc...

	FSR_Lib_99_SrcShaders = ____fsr_lib_counter_begin + 99 * 3,

	FSR_Absolute,

	FSR_Count
};

enum SeekDir
{
	SEEK_DIR_BEGIN = 0,
	SEEK_DIR_CUR,
	SEEK_DIR_END,
};

/// Abstract stream for reading
class  Deserializer
{
public:
	Deserializer();
	Deserializer(unsigned size);
	virtual ~Deserializer();

	virtual unsigned Read(void* dest, unsigned size) = 0;
	virtual unsigned Seek(unsigned position, SeekDir seekDir = SEEK_DIR_BEGIN) = 0;
	virtual const String& GetName() const = 0;
	virtual unsigned GetChecksum();

	unsigned GetPosition() const { return mPosition; }
	unsigned GetSize() const { return mSize; }
	bool IsEof() const { return mPosition >= mSize; }
	int64_t ReadInt64();
	int ReadInt();
	int16_t ReadShort();
	int8_t ReadByte();
	unsigned ReadUInt();
	uint16_t ReadUShort();
	uint8_t ReadUByte();
	bool ReadBool();
	float ReadFloat();
	double ReadDouble();
	float2 ReadVector2();
	float3 ReadVector3();
	float3 ReadPackedVector3(float maxAbsCoord);
	float4 ReadVector4();
	String ReadString();
	String ReadFileID();
	String ReadLine();

protected:
	unsigned mPosition;
	unsigned mSize;
};

/// Abstract stream for writing
class  Serializer
{
public:
	virtual ~Serializer();

	virtual unsigned Write(const void* data, unsigned size) = 0;

	bool WriteInt64(int64_t value);
	bool WriteInt(int value);
	bool WriteShort(int16_t value);
	bool WriteByte(int8_t value);
	bool WriteUInt(unsigned value);
	bool WriteUShort(uint16_t value);
	bool WriteUByte(uint8_t value);
	bool WriteBool(bool value);
	bool WriteFloat(float value);
	bool WriteDouble(double value);
	bool WriteVector2(const float2& value);
	bool WriteVector3(const float3& value);
	bool WritePackedVector3(const float3& value, float maxAbsCoord);
	bool WriteVector4(const float4& value);
	bool WriteString(const String& value);
	bool WriteFileID(const String& value);
	bool WriteLine(const String& value);
};

/// Text / binary file loaded from disk
class File : public Deserializer, public Serializer
{
public:
	File();

	bool Open(const String& fileName, FileMode mode, FSRoot root);
	void Close();
	void Flush();

	unsigned Read(void* dest, unsigned size) override;
	unsigned Seek(unsigned position, SeekDir seekDir = SEEK_DIR_BEGIN) override;
	unsigned Write(const void* data, unsigned size) override;

	String ReadText();

	unsigned GetChecksum() override;

	const String& GetName() const override { return mFileName; }
	FileMode GetMode() const { return mMode; }
	bool IsOpen() const { return pHandle != NULL; }
	bool IsReadOnly() const { return mMode == FileMode::FM_Read || mMode == FileMode::FM_ReadBinary; }
	bool IsWriteOnly() const { return mMode == FileMode::FM_Write || mMode == FileMode::FM_WriteBinary; }
	void* GetHandle() const { return pHandle; }

protected:
	String mFileName;
	FileMode mMode;
	FileHandle pHandle;
	unsigned mOffset;
	unsigned mChecksum;
	bool mReadSyncNeeded;
	bool mWriteSyncNeeded;
};

/// Memory area simulating a stream
class  MemoryBuffer : public Deserializer, public Serializer
{
public:
	MemoryBuffer(void* data, unsigned size);
	MemoryBuffer(const void* data, unsigned size);

	const String& GetName() const override { return mName; }

	unsigned Read(void* dest, unsigned size) override;
	unsigned Seek(unsigned position, SeekDir seekDir = SEEK_DIR_BEGIN) override;
	unsigned Write(const void* data, unsigned size) override;

	unsigned char* GetData() { return pBuffer; }
	bool IsReadOnly() { return mReadOnly; }

private:
	String mName;
	unsigned char* pBuffer;
	bool mReadOnly;
};

/// High level platform independent file system
class FileSystem
{
public:
	static unsigned	GetFileSize(FileHandle handle);
	// Allows to modify root paths at runtime
	static void		SetRootPath(FSRoot root, const String& rootPath);
	// Reverts back to App static defined pszRoots[]
	static void		ClearModifiedRootPaths();
	static unsigned	GetLastModifiedTime(const String& _fileName);
	// First looks it root exists in m_ModifiedRootPaths
	// otherwise uses App static defined pszRoots[]
	static String	FixPath(const String& pszFileName, FSRoot root);
    static bool		FileExists(const String& pszFileName, FSRoot root);

	static String	GetCurrentDir() { return AddTrailingSlash(_getCurrentDir()); }
	static String	GetProgramDir() { return GetPath(_getExePath()); }
	static String	GetUserDocumentsDir() { return AddTrailingSlash(_getUserDocumentsDir()); }
	static String	GetAppPreferencesDir(const String& org, const String& app) { return AddTrailingSlash(_getAppPrefsDir(org, app)); }

	static void		SetCurrentDir(const String& path) { _setCurrentDir(path.c_str()); }

	static void		SplitPath(const String& fullPath, String* pathName, String* fileName, String* extension, bool lowercaseExtension = true);
	static String	GetPath(const String& fullPath);
	static String	GetFileName(const String& fullPath);
	static String	GetExtension(const String& fullPath, bool lowercaseExtension = true);
	static String	GetFileNameAndExtension(const String& fullPath, bool lowercaseExtension = false);
	static String	ReplaceExtension(const String& fullPath, const String& newExtension);
	static String	AddTrailingSlash(const String& pathName);
	static String	RemoveTrailingSlash(const String& pathName);
	static String	GetParentPath(const String& pathName);
	static String	GetInternalPath(const String& pathName);
	static String	GetNativePath(const String& pathName);

	static bool		DirExists(const String& pathName);
	static bool		CreateDir(const String& pathName);
	static int		SystemRun(const String& fileName, const tinystl::vector<String>& arguments, String stdOut = "");
	static bool		Delete(const String& fileName);

private:
	// The following root paths are the ones that were modified at run-time
	static String	mModifiedRootPaths[FSRoot::FSR_Count];
	static String	mProgramDir;
};
