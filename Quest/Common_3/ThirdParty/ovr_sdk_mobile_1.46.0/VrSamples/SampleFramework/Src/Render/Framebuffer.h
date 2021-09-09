/************************************************************************************

Filename  	: 	Framebuffer.h
Content		: 	Frame buffer utilities. Originally part of the VrCubeWorld_NativeActivity
                sample.
Created		: 	March, 2015
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

 *************************************************************************************/

#pragma once

#include "VrApi.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "Render/Egl.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
   ================================================================================

   ovrFramebuffer

   ================================================================================
 */

typedef struct ovrFramebuffer_s {
    int Width;
    int Height;
    int Multisamples;
    int TextureSwapChainLength;
    int TextureSwapChainIndex;
    bool UseMultiview;
    ovrTextureSwapChain* ColorTextureSwapChain;
    GLuint* DepthBuffers;
    GLuint* FrameBuffers;
} ovrFramebuffer;

void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer);
bool ovrFramebuffer_Create(
    ovrFramebuffer* frameBuffer,
    const bool useMultiview,
    const GLenum colorFormat,
    const int width,
    const int height,
    const int multisamples);
void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer);
void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer);
void ovrFramebuffer_SetNone();
void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer);
void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer);

#if defined(__cplusplus)
} // extern "C"
#endif
