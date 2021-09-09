/************************************************************************************

Filename    :   GlSetup.h
Content     :   GL Setup
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once
#include "Egl.h"

namespace OVRFW {
struct glSetup_t {
    int glEsVersion; // 2 or 3
    EGLDisplay display;
    EGLSurface pbufferSurface; // use to make context current when we don't have window surfaces
    EGLConfig config;
    EGLContext context;
};

// Create an appropriate config, a tiny pbuffer surface, and a context,
// then make it current.  This combination can be used before and after
// the actual window surfaces are available.
// egl.context will be EGL_NO_CONTEXT if there was an error.
// requestedGlEsVersion can be 2 or 3.  If 3 is requested, 2 might still
// be returned in glSetup_t.glEsVersion.
//
// If contextPriority == EGL_CONTEXT_PRIORITY_MID_IMG, then no priority
// attribute will be set, otherwise EGL_CONTEXT_PRIORITY_LEVEL_IMG will
// be set with contextPriority.
glSetup_t GL_Setup(
    const EGLContext shareContext,
    const int requestedGlEsVersion,
    const int redBits,
    const int greenBits,
    const int blueBits,
    const int depthBits,
    const int multisamples,
    const GLuint contextPriority);

void GL_Shutdown(glSetup_t& glSetup);

// returns true if the application should exit
bool GL_ProcessEvents(int32_t& exitCode);

} // namespace OVRFW
