#pragma once

#if defined(GLES)
struct WindowHandle;
typedef void* GLContext;
typedef void* GLConfig;
typedef void* GLSurface;

bool initGL(GLConfig* pOutConfig);

void removeGL(GLConfig* pConfig);

bool initGLContext(GLConfig config, GLContext* pOutContext, GLContext sharedContext = nullptr);

void removeGLContext(GLContext* pContext);

bool addGLSurface(GLConfig context, GLConfig config, const WindowHandle* pWindowHandle, GLSurface* pOutSurface);

void removeGLSurface(GLSurface* pSurface);

bool swapGLBuffers(GLSurface surface);

void setGLSwapInterval(bool enableVsync);

void getGLSurfaceSize(unsigned int* width, unsigned int* height);

#endif