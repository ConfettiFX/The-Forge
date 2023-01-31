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

# pragma once

#include "../../Application/Config.h"

typedef enum OSDisplayServer {
	OS_DISPLAYSERVER_BUILTIN	= 1 << 0,  ///< Target OS builtin/default Displayserver, not available on Linux.
	OS_DISPLAYSERVER_WAYLAND 	= 1 << 1,  ///< Linux
	OS_DISPLAYSERVER_XLIB		= 1 << 2,  ///< Linux
	OS_DISPLAYSERVER_XCB		= 1 << 3,  ///< Linux
} OSDisplayServer;

inline const char* osDisplayServerName(OSDisplayServer pDS)
{
	switch (pDS) {
		case OS_DISPLAYSERVER_BUILTIN:
			return "Builtin";
		break;
		case OS_DISPLAYSERVER_WAYLAND:
			return "Wayland";
		break;
		case OS_DISPLAYSERVER_XLIB:
			return "Xlib";
		break;
		case OS_DISPLAYSERVER_XCB:
			return "Xcb";
		break;
	}
}

#if defined(FORGE_TARGET_LINUX)

//------------------------------------------------------------------------
// Implement me
//------------------------------------------------------------------------

/// @brief Indicates the default Displayserver on this target.
extern OSDisplayServer OS_DEFAULT_DISPLAYSERVER;

/// @brief Returns a flag containing all supported displayservers by the target.
FORGE_API int osDisplayServers();

/// @brief Returns a flag containing all compiled in displayservers.
FORGE_API int osDisplayServersCompiledIn();

//------------------------------------------------------------------------
// Header implemented
//------------------------------------------------------------------------

/// @brief Check if the given DisplayServer is supported by this OS.
inline bool osDisplayServerIsSupported(OSDisplayServer pCheck)
{
	return osDisplayServersCompiledIn() & pCheck && osDisplayServers() & pCheck;
}

#else

OSDisplayServer OS_DEFAULT_DISPLAYSERVER = OS_DISPLAYSERVER_BUILTIN;
inline int osDisplayServers() { return OS_DISPLAYSERVER_BUILTIN; }
inline int osDisplayServersCompiledIn() { return OS_DISPLAYSERVER_BUILTIN; }
inline bool osDisplayServerIsSupported(OSDisplayServer /* pCheck */)
{
	return true;
}

#endif
