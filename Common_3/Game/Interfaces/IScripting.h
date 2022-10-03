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

#ifndef ISCRIPTING_H
#define ISCRIPTING_H

#include "../../Application/Config.h"

// LUA
#include "../Scripting/LuaManager.h"

#ifdef ENABLE_FORGE_SCRIPTING
#define REGISTER_LUA_WIDGET(x) luaRegisterWidget((x))
#else
#define REGISTER_LUA_WIDGET(x) (void)(x)
#endif

/****************************************************************************/
// MARK: - Lua Scripting Data Structs
/****************************************************************************/

typedef struct LuaScriptDesc
{

	const char* pScriptFileName = NULL; 
	const char* pScriptFilePassword = NULL; 

} LuaScriptDesc;

/****************************************************************************/
// MARK: - Lua Scripting System Functionality
/****************************************************************************/

/// Destroys the internal LuaManager instance created by the interface
/// This allows the user to define their own custom LuaManager instance
/// This function MUST be called before a new LuaManager is instantiated
FORGE_API void luaDestroyCurrentManager();

/// Assigns custom user-defined LuaManager to be used internally 
/// MUST be called to define a new Manager after luaDestroyCurrentManager is invoked
FORGE_API void luaAssignCustomManager(LuaManager* pNewManager);

/// Adds an array of scripts to the Lua interface by filenames
FORGE_API void luaDefineScripts(LuaScriptDesc* pDescs, uint32_t count);

/// Add existing defined script to a queue to be executed 
/// Script execution will occur on next platform layer system update
FORGE_API void luaQueueScriptToRun(LuaScriptDesc* pDesc);

/// Register a Forge UI Widget for modification via a Lua script
FORGE_API void luaRegisterWidget(const void* pWidgetHandle);

#endif // ISCRIPTING_H