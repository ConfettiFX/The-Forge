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
#pragma once

//Use Virtual FileSystem
//#define USE_VFS

#include "../Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../ThirdParty/OpenSource/EASTL/vector.h"

typedef void* FileHandle;
typedef void (*FileDialogCallbackFn)(eastl::string url, void* userData);
/************************************************************************************************
* If you change this class please check the following projects to ensure you don't break them:  *
* 1) BlackFootBlade																			 *
* 2) Aura																					   *
* 3) Gladiator																				  *
* Also ask yourself if all of those projects will need the change, or just the one you're work- *
* ing on. If it is just the one you're working on please add it to an inherited version of the  *
* class within your project.																	*
************************************************************************************************/

/// Low level file system interface providing basic file I/O operations
/// Implementations platform dependent
FileHandle open_file(const char* filename, const char* flags);
bool       close_file(FileHandle handle);
void       flush_file(FileHandle handle);
size_t     read_file(void* buffer, size_t byteCount, FileHandle handle);
bool       seek_file(FileHandle handle, long offset, int origin);
long       tell_file(FileHandle handle);
size_t     write_file(const void* buffer, size_t byteCount, FileHandle handle);
time_t     get_file_last_modified_time(const char* _fileName);
time_t     get_file_last_accessed_time(const char* _fileName);
time_t     get_file_creation_time(const char* _fileName);

eastl::string get_current_dir();
eastl::string get_exe_path();
eastl::string get_app_prefs_dir(const char* org, const char* app);
eastl::string get_user_documents_dir();
void            get_files_with_extension(const char* dir, const char* ext, eastl::vector<eastl::string>& filesOut);
void            get_sub_directories(const char* dir, eastl::vector<eastl::string>& subDirectoriesOut);
bool            file_exists(const char* fileFullPath);
bool            absolute_path(const char* fileFullPath);
bool            copy_file(const char* src, const char* dst);
void            open_file_dialog(
			   const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
			   const eastl::vector<eastl::string>& fileExtensions);
void save_file_dialog(
	const char* title, const char* dir, FileDialogCallbackFn callback, void* userData, const char* fileDesc,
	const eastl::vector<eastl::string>& fileExtensions);
#if defined(TARGET_IOS)
void add_uti_to_map(const char* extension, const char* uti);
#endif

void set_current_dir(const char* path);

enum FileMode
{
	FM_Read = 1,
	FM_Write = FM_Read << 1,
	FM_Append = FM_Write << 1,
	FM_Binary = FM_Append << 1,
	FM_ReadWrite = FM_Read | FM_Write,
	FM_ReadAppend = FM_Read | FM_Append,
	FM_WriteBinary = FM_Write | FM_Binary,
	FM_ReadBinary = FM_Read | FM_Binary
};

// TODO : File System should be reworked so that we don't need all of these vague enums.
//	One suggesting is to just have a list of directories to check for each resource type.
//	Applications would then just need to add the paths they need to that list.
enum FSRoot
{
	// universal binary shader place
	FSR_BinShaders = 0,
	// the main applications shader source directory
	FSR_SrcShaders,
	// the main application texture source directory (TODO processed texture folder)
	FSR_Textures,

	FSR_Meshes,    // Meshes
	FSR_Builtin_Fonts,
	FSR_GpuConfig,
	FSR_Animation,    // Animation Ozz files
	FSR_OtherFiles,

	// libraries will want there own directories
	// support for upto 100 libraries
	____fsr_lib_counter_begin = FSR_OtherFiles,

	// Add libraries here
	FSR_Middleware0,
	FSR_Middleware1,
	FSR_Middleware2,

	____fsr_lib_counter_end = ____fsr_lib_counter_begin + 99 * 3,

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
class Deserializer
{
	public:
	Deserializer();
	Deserializer(unsigned size);
	virtual ~Deserializer();

	virtual unsigned               Read(void* dest, unsigned size) = 0;
	virtual unsigned               Seek(unsigned position, SeekDir seekDir = SEEK_DIR_BEGIN) = 0;
    virtual unsigned               Tell() = 0;
	virtual const eastl::string& GetName() const = 0;
	virtual unsigned               GetChecksum();

	unsigned        GetPosition() const { return mPosition; }
	unsigned        GetSize() const { return mSize; }
	bool            IsEof() const { return mPosition >= mSize; }
	int64_t         ReadInt64();
	int32_t         ReadInt();
	int16_t         ReadShort();
	int8_t          ReadByte();
	uint32_t        ReadUInt();
	uint16_t        ReadUShort();
	uint8_t         ReadUByte();
	bool            ReadBool();
	float           ReadFloat();
	double          ReadDouble();
	float2          ReadVector2();
	float3          ReadVector3();
	float3          ReadPackedVector3(float maxAbsCoord);
	float4          ReadVector4();
	eastl::string ReadString();
	eastl::string ReadFileID();
	eastl::string ReadLine();

	protected:
	unsigned mPosition;
	unsigned mSize;
};

/// Abstract stream for writing
class Serializer
{
	public:
	virtual ~Serializer();

	virtual unsigned Write(const void* data, unsigned size) = 0;

	bool WriteInt64(int64_t value);
	bool WriteInt(int32_t value);
	bool WriteShort(int16_t value);
	bool WriteByte(int8_t value);
	bool WriteUInt(uint32_t value);
	bool WriteUShort(uint16_t value);
	bool WriteUByte(uint8_t value);
	bool WriteBool(bool value);
	bool WriteFloat(float value);
	bool WriteDouble(double value);
	bool WriteVector2(const float2& value);
	bool WriteVector3(const float3& value);
	bool WritePackedVector3(const float3& value, float maxAbsCoord);
	bool WriteVector4(const float4& value);
	bool WriteString(const eastl::string& value);
	bool WriteFileID(const eastl::string& value);
	bool WriteLine(const eastl::string& value);
};

/// Text / binary file loaded from disk
class File: public Deserializer, public Serializer
{
	public:
	File();

	virtual bool Open(const eastl::string& fileName, FileMode mode, FSRoot root);
	virtual bool Close();
	virtual void Flush();

	unsigned Read(void* dest, unsigned size) override;
	unsigned Seek(unsigned position, SeekDir seekDir = SEEK_DIR_BEGIN) override;
    unsigned Tell() override;
	unsigned Write(const void* data, unsigned size) override;

	eastl::string ReadText();

	virtual unsigned             GetChecksum() override;
	virtual const eastl::string& GetName() const override { return mFileName; }
	virtual FileMode             GetMode() const { return mMode; }
	virtual bool                 IsOpen() const { return pHandle != NULL; }
	virtual bool                 IsReadOnly() const { return !(mMode & FileMode::FM_Write || mMode & FileMode::FM_Append); }
	virtual bool                 IsWriteOnly() const { return !(mMode & FileMode::FM_Read); }
	virtual void*                GetHandle() const { return pHandle; }

	protected:
	eastl::string mFileName;
	FileMode        mMode;
	FileHandle      pHandle;
	unsigned        mOffset;
	unsigned        mChecksum;
	bool            mReadSyncNeeded;
	bool            mWriteSyncNeeded;
};

/// Memory area simulating a stream
class MemoryBuffer: public Deserializer, public Serializer
{
	public:
	MemoryBuffer(void* data, unsigned size);
	MemoryBuffer(const void* data, unsigned size);

	const eastl::string& GetName() const override { return mName; }

	unsigned Read(void* dest, unsigned size) override;
	unsigned Seek(unsigned position, SeekDir seekDir = SEEK_DIR_BEGIN) override;
    unsigned Tell() override;
	unsigned Write(const void* data, unsigned size) override;

	unsigned char* GetData() { return pBuffer; }
	bool           IsReadOnly() { return mReadOnly; }

	private:
	eastl::string mName;
	unsigned char*  pBuffer;
	bool            mReadOnly;
};

/// High level platform independent file system
class FileSystem
{
	public:
	static unsigned GetFileSize(FileHandle handle);
	// Allows to modify root paths at runtime
	static void SetRootPath(FSRoot root, const eastl::string& rootPath);
	// Reverts back to App static defined pszRoots[]
	static void     ClearModifiedRootPaths();
	static time_t GetLastModifiedTime(const eastl::string& _fileName);
	static time_t GetLastAccessedTime(const eastl::string& _fileName);
	static time_t GetCreationTime(const eastl::string& _fileName);

	// First looks it root exists in m_ModifiedRootPaths
	// otherwise uses App static defined pszRoots[]
	static eastl::string FixPath(const eastl::string& pszFileName, FSRoot root);
	static eastl::string GetRootPath(FSRoot root);
	static bool            FileExists(const eastl::string& pszFileName, FSRoot root);

	static eastl::string GetCurrentDir() { return AddTrailingSlash(get_current_dir()); }
	static eastl::string GetProgramDir() { return GetPath(get_exe_path()); }
	static eastl::string GetProgramFileName() { return GetFileName(get_exe_path()); }
	static eastl::string GetUserDocumentsDir() { return AddTrailingSlash(get_user_documents_dir()); }
	static eastl::string GetAppPreferencesDir(const eastl::string& org, const eastl::string& app)
	{
		return AddTrailingSlash(get_app_prefs_dir(org.c_str(), app.c_str()));
	}
	static void GetFilesWithExtension(const eastl::string& dir, const eastl::string& ext, eastl::vector<eastl::string>& files)
	{
		get_files_with_extension(dir.c_str(), ext.c_str(), files);
	}
	static void GetSubDirectories(const eastl::string& dir, eastl::vector<eastl::string>& subDirectories)
	{
		get_sub_directories(dir.c_str(), subDirectories);
	}

	static void SetCurrentDir(const eastl::string& path) { set_current_dir(path.c_str()); }

	static eastl::string CombinePaths(const eastl::string& path1, const eastl::string& path2);
	static void SplitPath(
		const eastl::string& fullPath, eastl::string* pathName, eastl::string* fileName, eastl::string* extension,
		bool lowercaseExtension = true);
	static eastl::string GetPath(const eastl::string& fullPath);
	static eastl::string GetFileName(const eastl::string& fullPath);
	static eastl::string GetExtension(const eastl::string& fullPath, bool lowercaseExtension = true);
	static eastl::string GetFileNameAndExtension(const eastl::string& fullPath, bool lowercaseExtension = false);
	static eastl::string ReplaceExtension(const eastl::string& fullPath, const eastl::string& newExtension);
	static eastl::string AddTrailingSlash(const eastl::string& pathName);
	static eastl::string RemoveTrailingSlash(const eastl::string& pathName);
	static eastl::string GetParentPath(const eastl::string& pathName);
	static eastl::string GetInternalPath(const eastl::string& pathName);
	static eastl::string GetNativePath(const eastl::string& pathName);

	static bool CopyFile(const eastl::string& src, const eastl::string& dst, bool bFailIfExists);
	static bool DirExists(const eastl::string& pathName);
	static bool CreateDir(const eastl::string& pathName);
	static int  SystemRun(const eastl::string& fileName, const eastl::vector<eastl::string>& arguments, eastl::string stdOut = "");
	static bool Delete(const eastl::string& fileName);

	static void OpenFileDialog(
		const eastl::string& title, const eastl::string& dir, FileDialogCallbackFn callback, void* userData,
		const eastl::string& fileDesc, const eastl::vector<eastl::string>& allowedExtentions)
	{
		open_file_dialog(title.c_str(), dir.c_str(), callback, userData, fileDesc.c_str(), allowedExtentions);
	}

	static void SaveFileDialog(
		const eastl::string& title, const eastl::string& dir, FileDialogCallbackFn callback, void* userData,
		const eastl::string& fileDesc, const eastl::vector<eastl::string>& allowedExtentions)
	{
		save_file_dialog(title.c_str(), dir.c_str(), callback, userData, fileDesc.c_str(), allowedExtentions);
	}


	class Watcher
	{
		public:
		struct Data;
		enum
		{
			EVENT_MODIFIED = 1,
			EVENT_ACCESSED = 2,
			EVENT_CREATED = 4,
			EVENT_DELETED = 8,
		};
		typedef void (*Callback)(const char* path, uint32_t action);

		Watcher(const char* pWatchPath, FSRoot root, uint32_t eventMask, Callback callback);
		~Watcher();

		private:
		Data* pData;
	};

	private:
	// The following root paths are the ones that were modified at run-time
	static eastl::string mModifiedRootPaths[FSRoot::FSR_Count];
	static eastl::string mProgramDir;
};
