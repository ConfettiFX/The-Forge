/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

#include "../../Common_3/OS/Interfaces/IMiddleware.h"

// forward decls
struct Renderer;
struct RenderTarget;
struct Texture;
struct Cmd;
struct GpuProfiler;
struct Queue;
struct Gui;

/************************************************************************/
/*						 HOW TO USE THIS MODULE
/************************************************************************/
//
// - Init()		should be called at the end of App.Init() from the calling app. There are two options for initializing Panini Projection post process:
//		[1] (DYNAMIC_MODE) Init() with GUI controls. The Panini Parameters can be dynamically changed from the GUI.
//		[2]  (STATIC_MODE) Init() with panini parameters. This mode doesn't support changing of parameters through GUI. However, the
//								  user can call updatePaniniParameters() to change panini parameters if its initialized without a GUI.
//
// - Load() / Unload() are empty functions for Panini Projection as the App is responsible for managing Render Targets
//
// - Update()	should be called if the Panini Post Process is initialized with a GUI to update the dynamic GUI 
//				controls and the projection parameters
//
// - Draw()		should be called before drawing the GUI and after drawing the scene into an intermediate render target, whose pTexture
//				is passed as an input parameter
//
// - Exit()		should be called when exiting the app to clean up Panini rendering resources.
//
// Panini Post Process takes a texture as input which contains the rendered scene, applies Panini distortion to it and outputs to currently bound render target. 
// See UnitTests - 04_ExecuteIndirect project for example use case for this module.
//



/************************************************************************/
/*						 PANINI PROJECTION
/************************************************************************/
//
// The Pannini projection is a mathematical rule for constructing perspective images with very wide fields of view. 
// source:	http://tksharpless.net/vedutismo/Pannini/
// paper:   http://tksharpless.net/vedutismo/Pannini/panini.pdf
//
struct PaniniParameters
{
	// horizontal field of view in degrees
	float FoVH = 90.0f;

	// D parameter: Distance of projection's center from the Panini frame's origin.
	//				i.e. controls horizontal compression.
	//				D = 0.0f    : regular rectilinear projection
	//				D = 1.0f    : Panini projection
	//				D->Infinity : cylindrical orthographic projection
	float D = 1.0f;

	// S parameter: A scalar that controls 'Hard Vertical Compression' of the projection.
	//				Panini projection produces curved horizontal lines, which can feel
	//				unnatural. Vertical compression attempts to straighten those curved lines.
	//				S parameter works for FoVH < 180 degrees
	//				S = 0.0f	: No compression
	//				S = 1.0f	: Full straightening
	float S = 0.0f;

	// scale:		Rendering scale to fit the distorted image to the screen. The bigger
	//				the FoVH, the bigger the distortion is, hence the bigger the scale should 
	//				be in order to fit the image to screen.
	float scale = 1.0f;
};


/************************************************************************/
/*						 INTERFACE
/************************************************************************/
class AppPanini : public IMiddleware
{
public:


	// our init function should only be called once
	// the middleware has to keep these pointers
	virtual bool Init(Renderer* const renderer, Queue* const gfxQ, Queue* const cmpQ, Gui* const gui, GpuProfiler* const profiler) final;
	virtual void Exit() final;

	// when app is loaded, app is provided of the render targets to load
	// app is responsible to keep track of these render targets until load is called again
	// app will use the -first- rendertarget as texture to render to
	// make sure to always supply at least one render target with texture!
	virtual bool Load(RenderTarget** rts) final;
	virtual void Unload() final;

	// draws Panini Projection into first render target supplied at the Load call
	virtual void Draw(Cmd* cmd) final;

	virtual void Update(float deltaTime) final;

	// Sets the callback for enabling/disabling panini projection
	// @pfnPaniniToggleCallback	: callback function for handling rendering resources when Panini projection is enabled/disabled. In
	//							  an ideal case, we would directly use the swapchain buffer in the App if Panini post process is disabled to
	//							  avoid redundant draw calls / render target copies. This function reports back if the post process is enabled/disabled.
	bool SetCallbackToggle(void(*pfnPaniniToggleCallback)(bool));


	// Set FOV ptr
	void SetFovPtr(float* pFieldOfView);

	// Set texture to render to
	void SetSourceTexture(Texture* pTex);

};
