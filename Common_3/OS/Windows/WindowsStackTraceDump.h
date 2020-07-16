#pragma once
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IThread.h"

struct WindowsStackTraceLineInfo
{
	char  mFunctionName[512];
	char  mFileName[512];
	DWORD mLineNumber;
};

class WindowsStackTrace
{
public:
	static WindowsStackTrace* pInst;

	Mutex  mDbgHelpMutex;
	size_t mUsedMemorySize;
	size_t mPreallocatedMemorySize;
	void*  pPreallocatedMemory;

	static bool Init();
	static void Exit();
	static void* Alloc(size_t size);
	static LONG Dump(EXCEPTION_POINTERS* pExceptionInfo);
};

