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

#define MAX_FONTS 16

#include "../Core/Config.h"
#include "../Math/MathTypes.h"
#include "../Interfaces/ICameraController.h"

/****************************************************************************/
// MARK: - Public Font System Structs
/****************************************************************************/

/// Creation information for initializing Forge Rendering for fonts and text
typedef struct FontSystemDesc
{

	void*       pRenderer = NULL; // Renderer*
	uint32_t    mFontstashRingSizeBytes = 1024 * 1024;

} FontSystemDesc;

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
bool initFontSystem(FontSystemDesc* pDesc);

/// Frees Forge Rendering objects and memory associated with Font Rendering
/// To be called at application shutdown time by the App Layer
void exitFontSystem(); 

/// Creates graphics pipelines associated with Font Rendering
/// To be called at application load time by the App Layer
bool addFontSystemPipelines(void* /* RenderTarget** */ ppRenderTargets, uint32_t count, void* /* PipelineCache* */ pPipelineCache);

/// Destroys graphics pipelines associated with Font Rendering
/// To be called at application unload time by the App Layer
void removeFontSystemPipelines(); 

/// Renders UI-style text to the screen using a loaded font w/ The Forge
/// This function will assert if Font Rendering has not been initialized
void cmdDrawTextWithFont(void* /* Cmd* */ pCmd, float2 screenCoordsInPx, const FontDrawDesc* pDrawDesc);

/// Renders text as an object in the world using a loaded font w/ The Forge
/// This function will assert if Font Rendering has not been initialized
void cmdDrawWorldSpaceTextWithFont(void* /* Cmd* */ pCmd, const mat4* pMatWorld, const CameraMatrix* pMatProjView, const FontDrawDesc* pDrawDesc);

/****************************************************************************/
// MARK: - Other Font System Functionality
/****************************************************************************/

/// Loads an array of fonts from files and returns an array of their ID handles
void fntDefineFonts(const FontDesc* pDescs, uint32_t count, uint32_t* pOutIDs);

/// Returns the bounds of text that would be drawn to supplied specification
float2 fntMeasureFontText(const char* pText, const FontDrawDesc* pDrawDesc);

#endif // IFONT_H
