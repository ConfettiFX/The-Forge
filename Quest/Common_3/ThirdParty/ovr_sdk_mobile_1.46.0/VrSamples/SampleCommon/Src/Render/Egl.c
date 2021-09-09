/************************************************************************************

Filename	:	Egl.cpp
Content		: 	EGL utility functions. Originally part of the VrCubeWorld_NativeActivity
                sample.
Created		: 	March, 2015
Authors		: 	J.M.P. van Waveren
Language 	:	C99

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

 *************************************************************************************/

#include "Egl.h"
#include "Misc/Log.h"

#include <string.h>

//==============================================================================
// Implementation
//==============================================================================

PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_;
PFNEGLSIGNALSYNCKHRPROC eglSignalSyncKHR_;
PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_;
PFNGLINVALIDATEFRAMEBUFFER_ glInvalidateFramebuffer_;

/*
   ================================================================================

   OpenGL-ES Utility Functions

   ================================================================================
 */

OpenGLExtensions_t glExtensions;

void* GetExtensionProc(const char* functionName) {
    void* ptr = (void*)eglGetProcAddress(functionName);
    if (ptr == NULL) {
        ALOG("NOT FOUND: %s", functionName);
    }
    return ptr;
}

void EglInitExtensions() {
    const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
    if (allExtensions != NULL) {
        glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
            strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");

        glExtensions.EXT_texture_border_clamp =
            strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
            strstr(allExtensions, "GL_OES_texture_border_clamp");

        glExtensions.EXT_texture_filter_anisotropic =
            strstr(allExtensions, "GL_EXT_texture_filter_anisotropic");
    }

    eglCreateSyncKHR_ = (PFNEGLCREATESYNCKHRPROC)GetExtensionProc("eglCreateSyncKHR");
    eglDestroySyncKHR_ = (PFNEGLDESTROYSYNCKHRPROC)GetExtensionProc("eglDestroySyncKHR");
    eglClientWaitSyncKHR_ = (PFNEGLCLIENTWAITSYNCKHRPROC)GetExtensionProc("eglClientWaitSyncKHR");
    eglSignalSyncKHR_ = (PFNEGLSIGNALSYNCKHRPROC)GetExtensionProc("eglSignalSyncKHR");
    eglGetSyncAttribKHR_ = (PFNEGLGETSYNCATTRIBKHRPROC)GetExtensionProc("eglGetSyncAttribKHR");
    glInvalidateFramebuffer_ =
        (PFNGLINVALIDATEFRAMEBUFFER_)eglGetProcAddress("glInvalidateFramebuffer");
}

const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

const char* GlFrameBufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default:
            return "unknown";
    }
}

#ifdef CHECK_GL_ERRORS

static const char* GlErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "unknown";
    }
}

void GLCheckErrors(int line) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        ALOGE("GL error on line %d: %s", line, GlErrorString(error));
    }
}

#endif // CHECK_GL_ERRORS

bool GLCheckErrorsWithTitle(const char* logTitle) {
    bool hadError = false;

    // There can be multiple errors that need reporting.
    do {
        GLenum err = glGetError();
        if (err == GL_NO_ERROR) {
            break;
        }
        hadError = true;
        ALOGW(
            "%s GL Error: #0x%04x -> %s",
            (logTitle != NULL) ? logTitle : "<untitled>",
            (int)err,
            EglErrorString(err));
        if (err == GL_OUT_OF_MEMORY) {
            ALOGE_FAIL("GL_OUT_OF_MEMORY");
        }
    } while (1);
    return hadError;
}

EGLint GL_FlushSync(int timeout) {
    // if extension not present, return NO_SYNC
    if (eglCreateSyncKHR_ == NULL) {
        return EGL_FALSE;
    }

    EGLDisplay eglDisplay = eglGetCurrentDisplay();

    const EGLSyncKHR sync = eglCreateSyncKHR_(eglDisplay, EGL_SYNC_FENCE_KHR, NULL);
    if (sync == EGL_NO_SYNC_KHR) {
        return EGL_FALSE;
    }

    const EGLint wait =
        eglClientWaitSyncKHR_(eglDisplay, sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, timeout);

    eglDestroySyncKHR_(eglDisplay, sync);

    return wait;
}

void GL_Finish() {
    // Given the common driver "optimization" of ignoring glFinish, we
    // can't run reliably while drawing to the front buffer without
    // the Sync extension.
    if (eglCreateSyncKHR_ != NULL) {
        // 100 milliseconds == 100000000 nanoseconds
        const EGLint wait = GL_FlushSync(100000000);
        if (wait == EGL_TIMEOUT_EXPIRED_KHR) {
            ALOG("EGL_TIMEOUT_EXPIRED_KHR");
        }
        if (wait == EGL_FALSE) {
            ALOG("eglClientWaitSyncKHR returned EGL_FALSE");
        }
    }
}

void GL_Flush() {
    if (eglCreateSyncKHR_ != NULL) {
        const EGLint wait = GL_FlushSync(0);
        if (wait == EGL_FALSE) {
            ALOG("eglClientWaitSyncKHR returned EGL_FALSE");
        }
    }

    // Also do a glFlush() so it shows up in logging tools that
    // don't capture eglClientWaitSyncKHR_ calls.
    //	glFlush();
}

// This requires the isFBO parameter because GL ES 3.0's glInvalidateFramebuffer() uses
// different attachment values for FBO vs default framebuffer, unlike glDiscardFramebufferEXT()
void GL_InvalidateFramebuffer(
    const enum invalidateTarget_t isFBO,
    const bool colorBuffer,
    const bool depthBuffer) {
    const int offset = (int)!colorBuffer;
    const int count = (int)colorBuffer + ((int)depthBuffer) * 2;
    const GLenum fboAttachments[3] = {
        GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    const GLenum attachments[3] = {GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT};
    glInvalidateFramebuffer_(
        GL_FRAMEBUFFER, count, (isFBO == INV_FBO ? fboAttachments : attachments) + offset);
}

/*
   ================================================================================

   ovrEgl

   ================================================================================
 */

void ovrEgl_Clear(ovrEgl* egl) {
    egl->MajorVersion = 0;
    egl->MinorVersion = 0;
    egl->Display = 0;
    egl->Config = 0;
    egl->TinySurface = EGL_NO_SURFACE;
    egl->MainSurface = EGL_NO_SURFACE;
    egl->Context = EGL_NO_CONTEXT;
}

void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) {
    if (egl->Display != 0) {
        return;
    }

    egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
    eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8, // need alpha for the multi-pass timewarp compositor
        EGL_DEPTH_SIZE,
        0,
        EGL_STENCIL_SIZE,
        0,
        EGL_SAMPLES,
        0,
        EGL_NONE};
    egl->Config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }

        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(egl->Display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            egl->Config = configs[i];
            break;
        }
    }
    if (egl->Config == 0) {
        ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
    egl->Context = eglCreateContext(
        egl->Display,
        egl->Config,
        (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT,
        contextAttribs);
    if (egl->Context == EGL_NO_CONTEXT) {
        ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
    egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
    if (egl->TinySurface == EGL_NO_SURFACE) {
        ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
    ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
    if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) ==
        EGL_FALSE) {
        ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(egl->Display, egl->TinySurface);
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
}

void ovrEgl_DestroyContext(ovrEgl* egl) {
    if (egl->Display != 0) {
        ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ==
            EGL_FALSE) {
            ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (egl->Context != EGL_NO_CONTEXT) {
        ALOGE("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE) {
            ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Context = EGL_NO_CONTEXT;
    }
    if (egl->TinySurface != EGL_NO_SURFACE) {
        ALOGE("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE) {
            ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        egl->TinySurface = EGL_NO_SURFACE;
    }
    if (egl->Display != 0) {
        ALOGE("        eglTerminate( Display )");
        if (eglTerminate(egl->Display) == EGL_FALSE) {
            ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Display = 0;
    }
}
