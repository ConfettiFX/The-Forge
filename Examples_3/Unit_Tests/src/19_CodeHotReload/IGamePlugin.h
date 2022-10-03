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

#pragma once

#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"

// Defined here because this is shared by the Main and Game projects.
// Ideally it should be placed in some configuration file and disabled when not compiling with the editor.
#ifndef FORGE_CODE_HOT_RELOAD
#if defined(ANDROID) || defined(TARGET_IOS) || defined(XBOX) || defined(ORBIS) || defined(PROSPERO) || defined(NX64)
#define FORGE_CODE_HOT_RELOAD 0
#elif defined(_WINDOWS) || defined(__APPLE__) || defined(__linux__)
#define FORGE_CODE_HOT_RELOAD 1
#else
#error "Unsupported platform, please add it to the lists above."
#endif
#endif

// In a real case scenario the Engine would be compiled as a DLL so that the functionality in it
// could be shared with the plugin. As out setup for The Forge compile OS and Renderer to a static
// library we cannot do it (changing those to compile to a runtime library would be highly time consuming).
//
// For this reason in this example the functionality from the engine that needs to be shared with the Plugin is
// done using this structure.
struct EngineCallbacks
{
	void(*Log)(LogLevel, const char* msg);
};

struct GamePlugin;

// COMPONENTS 

struct AppDataComponent
{
	float deltaTime;
	float aspectRatio;

	Vector2 cameraPos;
	float cameraScale; // The bigger the camera the more we see

	Vector2 cameraMovementDir;
	float cameraZoom;

	GamePlugin* mGamePlugin;
};

struct WorldBoundsComponent
{
	float xMin, xMax, yMin, yMax;
};

struct PositionComponent
{
	Vector2 pos;
};

struct SpriteComponent
{
	float colorR, colorG, colorB;
	int   spriteIndex;
	float scale;
	float angle;
};

struct MoveComponent
{
	Vector2 vel;
};

// Functions from the hot reloadable module that are exposed to the engine.
struct GameCallbacks
{
	void (*UpdateCamera)(GamePlugin* plugin, float deltaTime);
	void (*UpdateWorldBounds)(WorldBoundsComponent* pWorldBounds, float deltaTime);
	void (*UpdateSprite)(SpriteComponent* sprite, float deltaTime);
};


struct GamePlugin
{
	EngineCallbacks* mEngine = nullptr;
	GameCallbacks* mGame = nullptr;

	AppDataComponent* mAppData = nullptr;
};
