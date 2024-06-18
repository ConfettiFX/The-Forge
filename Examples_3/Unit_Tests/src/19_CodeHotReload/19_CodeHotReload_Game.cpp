/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../../../../Common_3/Application/ThirdParty/OpenSource/cr/cr.h"

#include "IGamePlugin.h"

// The code in this file is compiled to a DLL and loaded by the main app (19_CodeHotReload_Main).
// While the program is running you can change the code in this file and load it in the app,
// depending on what OS you are running this test the process is a bit different:
//
//   - Windows/Linux: You can click "Rebuild Game" button in the UI.
//   - MacOS: You can click Command+B on XCode and it'll automatically rebuild your changes.
//
// Try searching for TRY_CODE_RELOAD in this file for things you can do to see this in action.
//
// Note that some of the examples we showcase in this demo for code hot reloading could also be exposed
// as checkboxes on the UI or data drive it, but in this case we do it in this way to showcase the
// code hot-reloding feature.

void updateCamera(GamePlugin* plugin, float deltaTime)
{
    Vector2 movementDir = plugin->mAppData->cameraMovementDir;

    // TRY_CODE_RELOAD
    // Enable this to invert the controls of the camera (WASD in keyboard and right stick with gamepad)
#if 0
	movementDir *= -1.f;
#endif

    // TRY_CODE_RELOAD
    // You can change the speed of the camera movement here
    const float speed = 10.f;
    const float zoomSpeed = 500.f;

    plugin->mAppData->cameraPos += movementDir * speed * deltaTime;

    const float zoomMult = 1.f + (deltaTime * zoomSpeed / 100);
    if (plugin->mAppData->cameraZoom > 0.f)
        plugin->mAppData->cameraScale *= zoomMult;
    else if (plugin->mAppData->cameraZoom < 0.f)
        plugin->mAppData->cameraScale /= zoomMult;
}

void updateWorldBounds(WorldBoundsComponent* pWorldBounds, float deltaTime)
{
    UNREF_PARAM(deltaTime);
    const float screenSizeX = 20.f;
    const float screenSizeY = 20.f;

    // TRY_CODE_RELOAD
    // Enable this to change the bounds of the world
#if 0
	static float elapsedTime = 0.f;
	elapsedTime += deltaTime;

	// TRY_CODE_RELOAD
	// Change the speed of the animation
	const float worldBoundsAnimationSeconds = 10.f;
	if (elapsedTime > worldBoundsAnimationSeconds)
		elapsedTime = 0.f;
	
	const float t = elapsedTime / worldBoundsAnimationSeconds;

	const float minSize = 5.f;
	const float sizeX = minSize + (screenSizeX - minSize) * t;
	const float sizeY = minSize + (screenSizeY - minSize) * t;

	pWorldBounds->xMin = -sizeX;
	pWorldBounds->xMax = sizeX;
	pWorldBounds->yMin = -sizeY;
	pWorldBounds->yMax = sizeY;
#else
    // Fixed bounds that fit screen size
    pWorldBounds->xMin = -screenSizeX;
    pWorldBounds->xMax = screenSizeX;
    pWorldBounds->yMin = -screenSizeY;
    pWorldBounds->yMax = screenSizeY;
#endif
}

void updateSprite(SpriteComponent* sprite, float deltaTime)
{
    UNREF_PARAM(sprite);
    UNREF_PARAM(deltaTime);
    // TRY_CODE_RELOAD
    // Enable to see all sprites rotate!
    // You can also change the rotation speed
#if 0
	const float rotationSpeedDeg = 30.f;
	sprite->angle += degToRad(rotationSpeedDeg) * deltaTime;
#endif
}

int onCodeLoad(struct cr_plugin* ctx)
{
    GamePlugin* gamePlugin = (GamePlugin*)ctx->userdata;

    // Currently we cannot call ASSERT or any other funtion from this project.
    // In order to support asserts we would need to compile OS into a dll and link it with this module,
    // as all our unit tests are already setup to use OS as static library doint that would be a mayor
    // change and we prefer to keep it this way.
#if 0
	ASSERT(gamePlugin);
	ASSERT(gamePlugin->mEngine);
	ASSERT(gamePlugin->mGame);
#endif

    gamePlugin->mEngine->Log(LogLevel::eINFO, "Loading Game Plugin.");

    gamePlugin->mGame->UpdateCamera = updateCamera;
    gamePlugin->mGame->UpdateWorldBounds = updateWorldBounds;
    gamePlugin->mGame->UpdateSprite = updateSprite;
    return 0;
}

int onCodeUnload(struct cr_plugin* ctx)
{
    GamePlugin* gamePlugin = (GamePlugin*)ctx->userdata;

    gamePlugin->mEngine->Log(LogLevel::eINFO, "Unoading Game Plugin for hot reload.");

    *gamePlugin->mGame = GameCallbacks{}; // Make sure no one tries to call unloaded functions.
    return 0;
}

int onCodeClose(struct cr_plugin* ctx)
{
    UNREF_PARAM(ctx);
    return 0;
}

int onCodeUpdate(struct cr_plugin* ctx)
{
    UNREF_PARAM(ctx);
    // GamePlugin* gamePlugin = (GamePlugin*)ctx->userdata;

    return 0;
}

#if FORGE_CODE_HOT_RELOAD
FORGE_EXTERN_C FORGE_EXPORT
#endif
    int
    tfMainCodeReload(struct cr_plugin* ctx, enum cr_op operation)
{
    switch (operation)
    {
    case CR_LOAD:
        return onCodeLoad(ctx); // loading back from a reload
    case CR_UNLOAD:
        return onCodeUnload(ctx); // preparing to a new reload
    case CR_CLOSE:
        return onCodeClose(ctx); // the plugin will close and not reload anymore
    case CR_STEP:
        return onCodeUpdate(ctx);
    }

    // We shouldn't reach this point
    return -1;
}
