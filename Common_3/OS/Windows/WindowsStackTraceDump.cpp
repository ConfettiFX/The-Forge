#include "WindowsStackTraceDump.h"
#include "../Interfaces/ILog.h"
#pragma warning (push)
#pragma warning( disable : 4091 )
#include <DbgHelp.h>
#pragma warning (pop)
#pragma comment (lib, "DbgHelp.lib")
#include <Psapi.h>

WindowsStackTrace* WindowsStackTrace::pInst;

static LONG WINAPI dumpStackTrace(EXCEPTION_POINTERS* pExceptionInfo)
{
	return WindowsStackTrace::Dump(pExceptionInfo);
}

bool WindowsStackTrace::Init()
{
	if (pInst)
	{
		return false;
	}

	ULONG stackSize = 20000;
	if (!SetThreadStackGuarantee(&stackSize))
		return false;

	SetUnhandledExceptionFilter(&dumpStackTrace);

	pInst = tf_new(WindowsStackTrace);
	pInst->mDbgHelpMutex.Init();
	pInst->mUsedMemorySize = 0;
	pInst->mPreallocatedMemorySize = 1024LL * 1024LL;
	pInst->pPreallocatedMemory = tf_calloc(1, pInst->mPreallocatedMemorySize);

	return true;
}

void WindowsStackTrace::Exit()
{
	if (pInst)
	{
		tf_free(pInst->pPreallocatedMemory);
		pInst->mDbgHelpMutex.Destroy();
		tf_delete(pInst);
		pInst = NULL;
	}
}

void * WindowsStackTrace::Alloc(size_t size)
{
	if (!pInst)
	{
		LOGF(LogLevel::eERROR, "StackTrace instance is not initialized");
		return NULL;
	}

	if ((pInst->mUsedMemorySize + size) > pInst->mPreallocatedMemorySize)
	{
		LOGF(LogLevel::eERROR, "Not enough preallocated memory for dump stack trace");
		return NULL;
	}

	void* pMem = ((uint8_t*)pInst->pPreallocatedMemory + pInst->mUsedMemorySize);
	pInst->mUsedMemorySize += size;
	return pMem;
}

LONG WindowsStackTrace::Dump(EXCEPTION_POINTERS * pExceptionInfo)
{
	if (!pInst)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}

	MutexLock dbgHelpLock(pInst->mDbgHelpMutex);

	LOGF(LogLevel::eERROR, "APP CRASHED - See the stack trace below");

	HANDLE thread = GetCurrentThread();
	HANDLE process = GetCurrentProcess();

	SymInitialize(process, NULL, FALSE);
	DWORD options = SymGetOptions() | (SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

	HMODULE* pModuleHandles;
	DWORD requiredSize;
	EnumProcessModules(process, NULL, 0, &requiredSize);
	DWORD numModules = requiredSize / sizeof(*pModuleHandles);
	pModuleHandles = (HMODULE*)WindowsStackTrace::Alloc(requiredSize);
	EnumProcessModules(process, pModuleHandles, requiredSize, &requiredSize);

	void* moduleBaseAddress = NULL;
	for (DWORD i = 0; i < numModules; ++i)
	{
		MODULEINFO moduleInfo;
		GetModuleInformation(process, pModuleHandles[i], &moduleInfo, sizeof(moduleInfo));

		if (!moduleBaseAddress)
		{
			moduleBaseAddress = moduleInfo.lpBaseOfDll;
		}

		char imageName[512] = {};
		GetModuleFileNameExA(process, pModuleHandles[i], imageName, sizeof(imageName));
		char moduleName[512] = {};
		GetModuleBaseNameA(process, pModuleHandles[i], moduleName, sizeof(moduleName));
		SymLoadModule64(process, 0, imageName, moduleName, (DWORD64)moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);
	}

	CONTEXT context = *(pExceptionInfo->ContextRecord);
	STACKFRAME64 stackFrame = {};
#ifdef _M_IX86
	stackFrame.AddrPC.Offset = context.Eip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Esp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Ebp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
#else
	stackFrame.AddrPC.Offset = context.Rip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Rsp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Rbp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
#endif

	IMAGE_NT_HEADERS* pImageHeader = ImageNtHeader(moduleBaseAddress);
	DWORD imageType = pImageHeader->FileHeader.Machine;

	const DWORD maxLength = 1024;
	uint8_t symbolMem[sizeof(IMAGEHLP_SYMBOL64) + maxLength];
	IMAGEHLP_SYMBOL64* pSymbol = (IMAGEHLP_SYMBOL64*)symbolMem;

	DWORD maxFunctionNameLength = 0;
	int maxNumLines = 100;
	int numLines = 0;
	WindowsStackTraceLineInfo* stackTraceLines = (WindowsStackTraceLineInfo*)WindowsStackTrace::Alloc(maxNumLines * sizeof(*stackTraceLines));
	do
	{
		if (!StackWalk64(imageType, process, thread, &stackFrame, &context, NULL, &SymFunctionTableAccess64, &SymGetModuleBase64, NULL))
			break;

		if (stackFrame.AddrPC.Offset != 0)
		{
			if (numLines >= maxNumLines)
			{
				LOGF(LogLevel::eERROR, "You need to allocate more memory for stackTraceLines");
				break;
			}

			WindowsStackTraceLineInfo& lineInfo = stackTraceLines[numLines++];

			memset(pSymbol, 0, sizeof(symbolMem));
			pSymbol->SizeOfStruct = sizeof(*pSymbol);
			pSymbol->MaxNameLength = maxLength;
			DWORD64 displacement = 0;
			SymGetSymFromAddr64(process, stackFrame.AddrPC.Offset, &displacement, pSymbol);
			DWORD functionNameLength = UnDecorateSymbolName(pSymbol->Name, lineInfo.mFunctionName, sizeof(lineInfo.mFunctionName), UNDNAME_COMPLETE);
			if (maxFunctionNameLength < functionNameLength)
				maxFunctionNameLength = functionNameLength;

			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(line);
			DWORD offsetFromSymbol = 0;
#ifdef _M_IX86
			SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &offsetFromSymbol, &line);
#else
			SymGetLineFromAddr(process, stackFrame.AddrPC.Offset, &offsetFromSymbol, &line);
#endif
			if (line.FileName)
				strcpy(lineInfo.mFileName, line.FileName);
			lineInfo.mLineNumber = line.LineNumber;
		}
	} while (stackFrame.AddrReturn.Offset != 0);

	for (int i = 0; i < numLines; ++i)
	{
		DWORD padding = 5;
		WindowsStackTraceLineInfo& lineInfo = stackTraceLines[i];
		LOGF(LogLevel::eERROR, "%-*s | %s(%d)", maxFunctionNameLength + padding, lineInfo.mFunctionName, lineInfo.mFileName, lineInfo.mLineNumber);
	}

	SymCleanup(process);

	return EXCEPTION_EXECUTE_HANDLER;
}