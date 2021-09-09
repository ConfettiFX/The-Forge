/************************************************************************************

Filename	: 	Egl.h
Content		: 	EGL utility functions. Originally part of the VrCubeWorld_NativeActivity
                sample.
Created		: 	March, 2015
Authors		: 	J.M.P. van Waveren
Language	:	C99
Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

 *************************************************************************************/

#pragma once

#include <string.h>
#include <stdbool.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#if !defined(GL_EXT_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
#endif

#if !defined(GL_OVR_multiview)
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR 0x9630
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR 0x9632
#define GL_MAX_VIEWS_OVR 0x9631
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#if !defined(GL_OVR_multiview_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLsizei samples,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

// EXT_sRGB_write_control
#if !defined(GL_EXT_sRGB_write_control)
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

#ifndef GL_EXT_discard_framebuffer
#define GL_EXT_discard_framebuffer 1
#define GL_COLOR_EXT 0x1800
#define GL_DEPTH_EXT 0x1801
#define GL_STENCIL_EXT 0x1802
#endif /* GL_EXT_discard_framebuffer */

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif /* GL_EXT_texture_filter_anisotropic */

#ifndef GL_OES_EGL_image_external
#define GL_OES_EGL_image_external 1
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#define GL_TEXTURE_BINDING_EXTERNAL_OES 0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES 0x8D68
#define GL_SAMPLER_EXTERNAL_OES 0x8D66
#endif /* GL_OES_EGL_image_external */

typedef struct ovrEgl_s {
    EGLint MajorVersion;
    EGLint MinorVersion;
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
} ovrEgl;

typedef struct {
    bool multi_view; // GL_OVR_multiview, GL_OVR_multiview2
    bool EXT_texture_border_clamp; // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
    bool EXT_texture_filter_anisotropic; // GL_EXT_texture_filter_anisotropic
} OpenGLExtensions_t;

extern OpenGLExtensions_t glExtensions;

//==============================================================================
// forward declarations
//==============================================================================

void EglInitExtensions();
void ovrEgl_Clear(ovrEgl* egl);
void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl);
void ovrEgl_DestroyContext(ovrEgl* egl);

// EGL_KHR_reusable_sync
extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_;
extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_;
extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_;
extern PFNEGLSIGNALSYNCKHRPROC eglSignalSyncKHR_;
extern PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_;

typedef void(GL_APIENTRYP PFNGLINVALIDATEFRAMEBUFFER_)(
    GLenum target,
    GLsizei numAttachments,
    const GLenum* attachments);
extern PFNGLINVALIDATEFRAMEBUFFER_ glInvalidateFramebuffer_;

// These use a KHR_Sync object if available, so drivers can't "optimize" the finish/flush away.
void GL_Finish();
void GL_Flush();

// Use EXT_discard_framebuffer or ES 3.0's glInvalidateFramebuffer as available
// This requires the isFBO parameter because GL ES 3.0's glInvalidateFramebuffer() uses
// different attachment values for FBO vs default framebuffer, unlike glDiscardFramebufferEXT()
enum invalidateTarget_t { INV_DEFAULT, INV_FBO };
void GL_InvalidateFramebuffer(
    const enum invalidateTarget_t isFBO,
    const bool colorBuffer,
    const bool depthBuffer);

const char* GlFrameBufferStatusString(GLenum status);
const char* EglErrorString(const EGLint error);

#ifdef OVR_BUILD_DEBUG
#define CHECK_GL_ERRORS 1
#endif

#ifdef CHECK_GL_ERRORS

void GLCheckErrors(int line);

#define GL(func) \
    func;        \
    GLCheckErrors(__LINE__);

#else // CHECK_GL_ERRORS

#define GL(func) func;

#endif // CHECK_GL_ERRORS

bool GLCheckErrorsWithTitle(const char* logTitle);

#if defined(__cplusplus)
} // extern "C"
#endif
