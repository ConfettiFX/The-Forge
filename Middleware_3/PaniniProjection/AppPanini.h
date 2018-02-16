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
class AppPanini
{
public:
	// INITIALIZES PANINI IN DYNAMNIC MODE
	//
	// initializes Panini Projection post process resources and adds dynamic properties 
	// to the GUI for changing Panini parameters during runtime. 
	//
	// @pRenderer				: pointer to the renderer required by the renderer interface functions
	// @pRenderTarget			: format of the render target is used for creating the pipeline state object
	// @pGuiWindow				: pointer to the GUI window to add the dynamic Panini controls from where
	//							  the user can change the Panini parameters during runtime
	// @pfnPaniniToggleCallback	: callback function for handling rendering resources when Panini projection is enabled/disabled. In
	//							  an ideal case, we would directly use the swapchain buffer in the App if Panini post process is disabled to
	//							  avoid redundant draw calls / render target copies. This function reports back if the post process is enabled/disabled.
	//
	bool Init(Renderer* pRenderer, RenderTarget* pRenderTarget, Gui* pGuiWindow, void(*pfnPaniniToggleCallback)(bool));

	// INITIALIZES PANINI IN STATIC MODE
	//
	// initializes Panini Projection post process resources and parameters. This function doesn't
	// configure Panini Projection post process to dynamically change the parameters through GUI.
	//
	// @pRenderer				: pointer to the renderer required by the renderer interface functions
	// @pRenderTarget			: format of the render target is used for creating the pipeline state object
	// @paniniParams			: sets the parameters for Panini post process during initialization. Call initPanini()
	//							  with the `Gui* pGuiWindow` parameter if you want GUI for changing panini parameters during runtime.
	//
	bool Init(Renderer* pRenderer, RenderTarget* pRenderTarget, PaniniParameters paniniParams);

	// releases resources of the Panini Projection post process. 
	// Exit() should be called before releasing the pRenderer.
	//
	void Exit(Renderer* pRenderer);


	// Panini Post Process doesn't have render target resources, hence empty Load/Unload functions.
	//
	bool Load() { return true; }
	void Unload() {}
	

	// Updates dynamic GUI controls and the projection parameters. updatePanini() should be 
	// called in the update loop of the App if the Panini post process is initialized using a GUI.
	//
	void Update(float* pFieldOfView);

	// draws Panini Projection into currently bound render target
	//
	// @cmd							: command list provided by the calling app. drawPanini() won't call cmdBegin() and cmdEnd()
	// @pPaniniInputTexture			: texture that contains the drawn scene
	// @pGraphicsGpuProfiler		: [OPTIONAL] GPU profiler specified by the calling app to display Panini performance numbers
	//
	void Draw(Cmd* cmd, Texture* pPaniniInputTexture, GpuProfiler* pGraphicsGpuProfiler = nullptr);

	//-----------------------------------------------------------------------------------------------------------------------------

	// Updates the Panini parameters. This is the only way to update Panini parameters if Panini is initialized in STATIC MODE. 
	// 
	void UpdateParameters(PaniniParameters paniniParams);
};
