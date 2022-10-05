#pragma once

#include "../GraphicsConfig.h"

#if defined(GLES)
struct WindowHandle;
typedef void* GLContext;
typedef void* GLConfig;
typedef void* GLSurface;

bool initGL(GLConfig* pOutConfig);

void removeGL(GLConfig* pConfig);

bool initGLContext(GLConfig config, GLContext* pOutContext, GLContext sharedContext = nullptr);

void removeGLContext(GLContext* pContext);

bool addGLSurface(GLContext context, GLConfig config, const WindowHandle* pWindowHandle, GLSurface* pOutSurface);

void removeGLSurface(GLContext context, GLConfig config, GLSurface* pSurface);

bool swapGLBuffers(GLSurface surface);

void setGLSwapInterval(bool enableVsync);

void getGLSurfaceSize(unsigned int* width, unsigned int* height);

void* getExtensionsFunction(const char* pExtFunctionName);

#endif