/*
* Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
* License: http://www.opensource.org/licenses/BSD-2-Clause
*/

#include "rmem_utils.h"

#if RMEM_PLATFORM_WINDOWS
#include "rmem_wrap_win.h"
#include <TlHelp32.h>
#endif

namespace rmem {

#if RMEM_PLATFORM_WINDOWS

	size_t getModuleInfo(uint8_t* _buffer)
	{
		const uint8_t charSize = 2;
		loadModuleFuncs();

		size_t buffPtr = 0;
		addVarToBuffer(charSize, _buffer, buffPtr);

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
		if (snapshot != INVALID_HANDLE_VALUE)
		{
			MODULEENTRY32W me;
			BOOL cap = Module32FirstW(snapshot, &me);
			if (!cap)
			{
				// fall back on enumerating modules
				HMODULE hMods[1024];
				DWORD cbNeeded;

				if (sFn_enumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded))
				{
					for (uint32_t i = 0; i<(cbNeeded / sizeof(HMODULE)); ++i)
					{
						wchar_t szModName[MAX_PATH];

						MODULEINFO mi;
						sFn_getModuleInformation(GetCurrentProcess(), hMods[i], &mi, sizeof(mi));

						if (sFn_getModuleFileNameExW(GetCurrentProcess(), hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
						{
							uint64_t modBase = (uint64_t)mi.lpBaseOfDll;
							uint64_t modSize = (uint64_t)mi.SizeOfImage;

							addStrToBuffer(szModName, _buffer, buffPtr, 0x23);
							addVarToBuffer(modBase, _buffer, buffPtr);
							addVarToBuffer(modSize, _buffer, buffPtr);
						}
					}
				}
			}
			else
				while (cap)
				{
					uint64_t modBase = (uint64_t)me.modBaseAddr;
					uint64_t modSize = (uint64_t)me.modBaseSize;

					addStrToBuffer(me.szExePath, _buffer, buffPtr, 0x23);
					addVarToBuffer(modBase, _buffer, buffPtr);
					addVarToBuffer(modSize, _buffer, buffPtr);

					cap = Module32NextW(snapshot, &me);
				}

			CloseHandle(snapshot);
		}

		return buffPtr;
	}

#elif RMEM_PLATFORM_XBOX360

	size_t getModuleInfo(uint8_t* _buffer)
	{
		const uint8_t charSize = 2;

		HRESULT error;
		PDM_WALK_MODULES pWalkMod = NULL;
		DMN_MODLOAD modLoad;
		size_t buffPtr = 0;
		addVarToBuffer(charSize, _buffer, buffPtr);

		while (XBDM_NOERR == (error = DmWalkLoadedModules(&pWalkMod, &modLoad)))
		{
			// Examine the contents of modLoad.
			uint64_t modBase = (uint64_t)(uint32_t)modLoad.BaseAddress;
			uint64_t modSize = (uint64_t)modLoad.Size;

			addStrToBuffer(modLoad.Name, _buffer, buffPtr, 0x23);
			addVarToBuffer(modBase, _buffer, buffPtr);
			addVarToBuffer(modSize, _buffer, buffPtr);
		}

		if (error != XBDM_ENDOFLIST)
			return 0;

		DmCloseLoadedModules(pWalkMod);

		return buffPtr;
	}

#elif RMEM_PLATFORM_ANDROID

	size_t getModuleInfo(uint8_t* _buffer)
	{
		const uint8_t charSize = 1;

		FILE* file = fopen("/proc/self/maps", "rt");
		if (!file)
			return 0;

		size_t buffPtr = 0;
		addVarToBuffer(charSize, _buffer, buffPtr);

		char buff[512];
		while (fgets(buff, sizeof(buff), file))
		{
			if (strstr(buff, "r-xp"))
			{
				size_t len = strlen(buff);
				buff[8] = 0;
				uint64_t modBase	= strtoul(buff, 0, 16);
				uint64_t modEnd		= strtoul(buff+9, 0, 16);

				char* modName = buff + len;
				while (*modName != ' ')
				{
					if ((*modName == '\n') || (*modName == '\r'))
						*modName = '\0';
					--modName;
				}
				++modName;

				addStrToBuffer(modName, _buffer, buffPtr, 0x23);
				addVarToBuffer(modBase, _buffer, buffPtr);
				addVarToBuffer(modEnd - modBase, _buffer, buffPtr);
			}
		}

		fclose(file);
		return buffPtr;
	}/*
* Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
* License: http://www.opensource.org/licenses/BSD-2-Clause
*/



#else 

	size_t getModuleInfo(uint8_t* _buffer)
	{
		(void)_buffer;
		return 0;
	}

#endif // RMEM_PLATFORM_WINDOWS

} // namespace rmem
