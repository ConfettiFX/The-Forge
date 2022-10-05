/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#if RMEM_PLATFORM_WINDOWS

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <Shlobj.h>
#include <Psapi.h>

typedef BOOL  (WINAPI * fnGetModuleInformation)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb);
typedef BOOL  (WINAPI * fnEnumProcessModules)(HANDLE hProcess, HMODULE* lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI * fnGetModuleFileNameExW)(HANDLE  hProcess, HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

static fnGetModuleInformation	sFn_getModuleInformation	= 0;
static fnEnumProcessModules		sFn_enumProcessModules		= 0;
static fnGetModuleFileNameExW	sFn_getModuleFileNameExW	= 0;

static void* loadFunc(HMODULE _kernel, HMODULE _psapi, const char* _name)
{
	FARPROC ret = ::GetProcAddress(_kernel, _name);
	if (!ret && (_psapi != 0))
		ret = ::GetProcAddress(_psapi, _name);
	return reinterpret_cast<void*>(ret);
}

static bool loadModuleFuncs()
{
	if (sFn_getModuleInformation)
		return true;

	HMODULE kerneldll32	= ::GetModuleHandleA("kernel32");
	HMODULE psapiDLL	= ::LoadLibraryA("Psapi.dll");

	sFn_getModuleInformation	= reinterpret_cast<fnGetModuleInformation>(loadFunc(kerneldll32, psapiDLL, "GetModuleInformation"));
	sFn_enumProcessModules		= reinterpret_cast<fnEnumProcessModules>  (loadFunc(kerneldll32, psapiDLL, "EnumProcessModules"));
	sFn_getModuleFileNameExW	= reinterpret_cast<fnGetModuleFileNameExW>(loadFunc(kerneldll32, psapiDLL, "GetModuleFileNameExW"));

	return ((sFn_getModuleInformation	!= 0) &&
			(sFn_enumProcessModules		!= 0) &&
			(sFn_getModuleFileNameExW	!= 0));
}

#endif // RMEM_PLATFORM_WINDOWS
