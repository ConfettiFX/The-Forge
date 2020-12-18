#if defined(GLES)

#include "GLESContextCreator.h"
#include "../../ThirdParty/OpenSource/OpenGL/GLES/egl.h"

#include "../../OS/Interfaces/ILog.h"
#include "../IRenderer.h"

#include "../../OS/Interfaces/IMemory.h"

void* gDisplay = nullptr;
thread_local EGLSurface gStartSurface = EGL_NO_SURFACE;
thread_local EGLint gSurfaceWidth;
thread_local EGLint gSurfaceHeight;

void util_egl_get_surface_size(GLSurface surface)
{
	if (eglQuerySurface(gDisplay, surface, EGL_WIDTH, &gSurfaceWidth) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to get EGL surface width! Error {%d}.", error);
		gSurfaceWidth = 0;
	}

	if (eglQuerySurface(gDisplay, surface, EGL_HEIGHT, &gSurfaceHeight) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to get EGL surface height! Error {%d}.", error);
		gSurfaceHeight = 0;
	}
}

bool initGL(GLConfig* pOutConfig)
{
	ASSERT(pOutConfig);

	// EGL setup
	gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint major, minor;
	if (eglInitialize(gDisplay, &major, &minor) == EGL_FALSE)
	{
		LOGF(LogLevel::eERROR, "Failed to initialize EGL!");
		return false;
	}

	if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
	{
		LOGF(LogLevel::eERROR, "Failed to bind EGL API!");
		return false;
	}

	LOGF(LogLevel::eINFO, "Initialized EGL %d.%d", major, minor);

	// Choose EGLConfig
	EGLint attrs[] =
	{
		EGL_RED_SIZE,           8,
		EGL_GREEN_SIZE,         8,
		EGL_BLUE_SIZE,          8,
		EGL_ALPHA_SIZE,         8,
		EGL_DEPTH_SIZE,			16,
		EGL_STENCIL_SIZE,		8,
		EGL_SAMPLES,			1,
		EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
		EGL_NONE
	};

	EGLint numconfigs;
	if (eglChooseConfig(gDisplay, attrs, pOutConfig, 1, &numconfigs) == EGL_FALSE)
	{
		LOGF(LogLevel::eERROR, "Could not select desired EGLConfig!");
		return false;
	}

	return true;
}

void removeGL(GLConfig* pConfig)
{
	ASSERT(gDisplay);
	ASSERT(pConfig);

	gDisplay = nullptr;
	*pConfig = nullptr;
}

bool initGLContext(GLConfig config, GLContext* pOutContext, GLContext sharedContext)
{
	ASSERT(gDisplay);
	ASSERT(config);
	ASSERT(pOutContext);

	EGLint attribList[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	*pOutContext = eglCreateContext(gDisplay, config, sharedContext, attribList);
	if (*pOutContext == EGL_NO_CONTEXT)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Could not create OpenGL context with EGL! Error: {%d}", error);
		return false;
	}

	EGLint surfaceAttribList[] =
	{
		EGL_HEIGHT, 480,
		EGL_WIDTH, 720,
		EGL_NONE
	};

	if(gStartSurface == EGL_NO_SURFACE)
		gStartSurface = eglCreatePbufferSurface(gDisplay, config, surfaceAttribList);

	if (eglMakeCurrent(gDisplay, gStartSurface, gStartSurface, *pOutContext) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to bind context to EGL surface! Error {%d}", error);
		return false;
	}
	
	return true;
}

void removeGLContext(GLContext* pContext)
{
	ASSERT(pContext);

	if (eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to unbind context from EGL! Error {%d}", error);
	}

	if (eglDestroyContext(gDisplay, *pContext) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to remove EGL context! Error {%d}", error);
	}

	if (gStartSurface != EGL_NO_SURFACE)
		removeGLSurface(&gStartSurface);

	*pContext = nullptr;
}

bool addGLSurface(GLConfig context, GLConfig config, const WindowHandle* pWindowHandle, GLSurface* pOutSurface)
{
	ASSERT(gDisplay);
	ASSERT(context);
	ASSERT(config);
	ASSERT(pWindowHandle);
	ASSERT(pOutSurface);

	EGLint surfaceAttributes[] = { EGL_NONE };
	*pOutSurface = eglCreateWindowSurface(gDisplay, config, pWindowHandle->window, surfaceAttributes);
	if (*pOutSurface == EGL_NO_SURFACE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to create EGL surface! Error {%d}", error);
		return false;
	}
	
	// Bind context
	if (eglMakeCurrent(gDisplay, *pOutSurface, *pOutSurface, context) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to bind context to EGL surface! Error {%d}", error);
		return false;
	}

	util_egl_get_surface_size(*pOutSurface);

	return true;
}

void removeGLSurface(GLSurface* pSurface)
{
	ASSERT(pSurface);
	if (eglDestroySurface(gDisplay, *pSurface) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to destroy EGL surface! Error {%d}", error);
	}
	*pSurface = nullptr;
	gSurfaceWidth = 0;
	gSurfaceHeight = 0;
}

bool swapGLBuffers(GLSurface surface)
{
	ASSERT(surface);
	ASSERT(gDisplay);

	if (eglSwapBuffers(gDisplay, surface) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to swap EGL buffers! Error {%d}", error);
		return false;
	}
	return true;
}

void setGLSwapInterval(bool enableVsync)
{
	ASSERT(gDisplay);
	if (eglSwapInterval(gDisplay, enableVsync ? 1 : 0) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to set swap interval! Error {%d}", error);
	}
}

void getGLSurfaceSize(unsigned int* width, unsigned int* height)
{
	if (width)
		*width = gSurfaceWidth;
	if (height)
		*height = gSurfaceHeight;
}

#endif