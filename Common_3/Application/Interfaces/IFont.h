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

#ifndef IFONT_H
#define IFONT_H

#include "../../Application/Config.h"
#include "../../Utilities/Math/MathTypes.h"
#include "../../Application/Interfaces/ICameraController.h"

typedef struct Renderer Renderer;
typedef struct Cmd Cmd;
typedef struct RenderTarget RenderTarget;
typedef struct PipelineCache PipelineCache;

/****************************************************************************/
// MARK: - Public Font System Structs
/****************************************************************************/

/// Creation information for initializing Forge Rendering for fonts and text
typedef struct FontSystemDesc
{

	Renderer*   pRenderer = NULL;
	uint32_t    mFontstashRingSizeBytes = 1024 * 1024;

} FontSystemDesc;

typedef struct FontSystemLoadDesc
{
	PipelineCache*  pCache;
	ReloadType      mLoadType;
	uint32_t        mColorFormat;  // enum TinyImageFormat
	uint32_t        mDepthFormat;  // enum TinyImageFormat
	uint32_t        mWidth;
	uint32_t        mHeight;
	uint32_t		mCullMode; // enum CullMode
	uint32_t		mDepthCompareMode; // enum CompareMode
} FontSystemLoadDesc;

/// Creation information for loading a font from a file using The Forge
typedef struct FontDesc
{

	const char* pFontName = "default";
	const char* pFontPath = NULL; 
	const char* pFontPassword = NULL; 

} FontDesc;

/// Aggregation of information necessary for drawing text with The Forge
typedef struct FontDrawDesc
{

	const char*   pText = NULL; 

	uint32_t      mFontID = 0;
	// Provided color should be A8B8G8R8_SRGB
	uint32_t      mFontColor = 0xffffffff;
	float         mFontSize = 16.0f;
	float         mFontSpacing = 0.0f;
	float         mFontBlur = 0.0f;

} FontDrawDesc;

/****************************************************************************/
// MARK: - Application Life Cycle 
/****************************************************************************/

/// Initializes Forge Rendering objects associated with Font Rendering
/// To be called at application initialization time by the App Layer
FORGE_API bool initFontSystem(FontSystemDesc* pDesc);

/// Frees Forge Rendering objects and memory associated with Font Rendering
/// To be called at application shutdown time by the App Layer
FORGE_API void exitFontSystem();

/// Loads Font Rendering resources depending on FontSystemLoadDesc::mLoadType
/// To be called at application load time by the App Layer
FORGE_API void loadFontSystem(const FontSystemLoadDesc* pDesc);

/// Unloads Font Rendering resources depending on FontSystemLoadDesc::mLoadType
/// To be called at application unload time by the App Layer
FORGE_API void unloadFontSystem(ReloadType unloadType);

/// Renders UI-style text to the screen using a loaded font w/ The Forge
/// This function will assert if Font Rendering has not been initialized
FORGE_API void cmdDrawTextWithFont(Cmd* pCmd, float2 screenCoordsInPx, const FontDrawDesc* pDrawDesc);

/// Renders text as an object in the world using a loaded font w/ The Forge
/// This function will assert if Font Rendering has not been initialized
FORGE_API void cmdDrawWorldSpaceTextWithFont(Cmd* pCmd, const mat4* pMatWorld, const CameraMatrix* pMatProjView, const FontDrawDesc* pDrawDesc);

/// Debugging feature - draws the contents of the internal font atlas
FORGE_API void cmdDrawDebugFontAtlas(Cmd* pCmd, float2 screenCoordsInPx);

/****************************************************************************/
// MARK: - Other Font System Functionality
/****************************************************************************/

/// Loads an array of fonts from files and returns an array of their ID handles
FORGE_API void fntDefineFonts(const FontDesc* pDescs, uint32_t count, uint32_t* pOutIDs);

/// Get current font atlas size
FORGE_API int2 fntGetFontAtlasSize();

/// Clear all data from the font atlas and resize it to provide size. Pass zero to keep the current size.
FORGE_API void fntResetFontAtlas(int2 newAtlasSize);

/// Expands the font atlas size by given size without clearing the contents
FORGE_API void fntExpandAtlas(int2 additionalSize);

/// Returns the bounds of text that would be drawn to supplied specification
FORGE_API float2 fntMeasureFontText(const char* pText, const FontDrawDesc* pDrawDesc);

#endif // IFONT_H
