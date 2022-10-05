#include "../GraphicsConfig.h"

#if defined(GLES)

#include "GLESContextCreator.h"
#include "../ThirdParty/OpenSource/OpenGL/GLES/egl.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IGraphics.h"

#include "../../Utilities/Interfaces/IMemory.h"

void* gDisplay = EGL_NO_DISPLAY;
thread_local EGLSurface gPlaceholderSurface = EGL_NO_SURFACE;
thread_local EGLint gSurfaceWidth;
thread_local EGLint gSurfaceHeight;

GLSurface util_egl_get_default_surface(GLConfig config)
{
	if (gPlaceholderSurface == EGL_NO_SURFACE)
	{
		EGLint surfaceAttribList[] =
		{
			EGL_HEIGHT, 4,
			EGL_WIDTH, 4,
			EGL_NONE
		};

		gPlaceholderSurface = eglCreatePbufferSurface(gDisplay, config, surfaceAttribList);
		if (gPlaceholderSurface == EGL_NO_SURFACE)
			LOGF(LogLevel::eERROR, "Failed to create default EGL! Error {%d}.", eglGetError());
	}

	return gPlaceholderSurface;
}

void util_egl_destroy_default_surface()
{
	if (gPlaceholderSurface != EGL_NO_SURFACE)
	{
		if (eglDestroySurface(gDisplay, gPlaceholderSurface) != EGL_TRUE)
			LOGF(LogLevel::eERROR, "Failed to destroy default EGL surface! Error {%d}", eglGetError());
		gPlaceholderSurface = EGL_NO_SURFACE;
	}
}

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
	if (gDisplay == EGL_NO_DISPLAY)
	{
		gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (gDisplay == EGL_NO_DISPLAY)
		{
			LOGF(LogLevel::eERROR, "Get EGL display failed!");
			return false;
		}

		EGLint major, minor;
		if (eglInitialize(gDisplay, &major, &minor) == EGL_FALSE)
		{
			LOGF(LogLevel::eERROR, "Failed to initialize EGL!");
			return false;
		}
		LOGF(LogLevel::eINFO, "Initialized EGL %d.%d", major, minor);
	}

	if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
	{
		LOGF(LogLevel::eERROR, "Failed to bind EGL API!");
		return false;
	}

	// Choose EGLConfig
	EGLint attrs[] =
	{
		EGL_RED_SIZE,           5,
		EGL_GREEN_SIZE,         6,
		EGL_BLUE_SIZE,          5,
		EGL_ALPHA_SIZE,         8,
		EGL_DEPTH_SIZE,         16,
		EGL_STENCIL_SIZE,		8,
		EGL_BUFFER_SIZE,		16,
		EGL_CONFIG_CAVEAT,		EGL_NONE,
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_RGB_BUFFER,
		EGL_CONFORMANT,			EGL_OPENGL_ES2_BIT,
		EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint numconfigs;
	if (eglChooseConfig(gDisplay, attrs, NULL, 0, &numconfigs) == EGL_FALSE)
	{
		LOGF(LogLevel::eERROR, "eglChooseConfig failed with error: %d", eglGetError());
		return false;
	}

	if (numconfigs < 1)
	{
		LOGF(LogLevel::eERROR, "eglChooseConfig could not find any matching configurations");
		return false;
	}

	EGLConfig* pPossibleEGLConfigs = (EGLConfig*)tf_calloc(numconfigs, sizeof(EGLConfig));

	if (eglChooseConfig(gDisplay, attrs, pPossibleEGLConfigs, numconfigs, &numconfigs) == EGL_FALSE)
	{
		LOGF(LogLevel::eERROR, "eglChooseConfig failed with error: %d", eglGetError());
		return false;
	}

	int32_t bestConfigIndex = -1;
	int32_t bestScore = 0;
	for (uint32_t i = 0; i < numconfigs; ++i)
	{
		EGLConfig config = pPossibleEGLConfigs[i];
		EGLint caveat, conformant, bufferSize, red, green, blue, alpha, alphaMask, depth, stencil, sampleBuffers, samples;
		eglGetConfigAttrib(gDisplay, config, EGL_CONFIG_CAVEAT, &caveat);
		eglGetConfigAttrib(gDisplay, config, EGL_CONFORMANT, &conformant);
		eglGetConfigAttrib(gDisplay, config, EGL_BUFFER_SIZE, &bufferSize);
		eglGetConfigAttrib(gDisplay, config, EGL_RED_SIZE, &red);
		eglGetConfigAttrib(gDisplay, config, EGL_GREEN_SIZE, &green);
		eglGetConfigAttrib(gDisplay, config, EGL_BLUE_SIZE, &blue);
		eglGetConfigAttrib(gDisplay, config, EGL_ALPHA_SIZE, &alpha);
		eglGetConfigAttrib(gDisplay, config, EGL_ALPHA_MASK_SIZE, &alphaMask);
		eglGetConfigAttrib(gDisplay, config, EGL_DEPTH_SIZE, &depth);
		eglGetConfigAttrib(gDisplay, config, EGL_STENCIL_SIZE, &stencil);
		eglGetConfigAttrib(gDisplay, config, EGL_SAMPLE_BUFFERS, &sampleBuffers);
		eglGetConfigAttrib(gDisplay, config, EGL_SAMPLES, &samples);

		bool isGoodConfig = (depth == 24 || depth == 16);
		isGoodConfig &= stencil == 8;
		if (!isGoodConfig)
			continue;
		uint32_t score = 0;

		// Android emulator gives configs only with samples equal to zero
		score += samples <= sampleBuffers ? samples : sampleBuffers;

		if ((bufferSize == 16) && (red == 5) && (green == 6) && (blue == 5) && (alpha == 0)) {
			score += 1;
		}
		else if ((bufferSize == 32) && (red == 8) && (green == 8) && (blue == 8) && (alpha == 0)) {
			score += 3;
		}
		else if ((bufferSize == 32) && (red == 8) && (green == 8) && (blue == 8) && (alpha == 8)) {
			score += 2;
		}
		else if ((bufferSize == 24) && (red == 8) && (green == 8) && (blue == 8) && (alpha == 0)) {
			score += 4;
		}

		score += (depth == 24 && stencil == 8);
		score += (conformant & EGL_OPENGL_ES2_BIT) == EGL_OPENGL_ES2_BIT;
		score += caveat == EGL_NONE;

		if (score > bestScore)
		{
			bestScore = score;
			bestConfigIndex = i;
		}
	}

	if (bestConfigIndex < 0)
	{
		LOGF(LogLevel::eERROR, "No correct matching egl configuration found");
		bestConfigIndex = 0;
	}

	*pOutConfig = pPossibleEGLConfigs[bestConfigIndex];

	tf_free(pPossibleEGLConfigs);

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

	GLSurface defaultSurface = util_egl_get_default_surface(config);

	if (eglMakeCurrent(gDisplay, defaultSurface, defaultSurface, *pOutContext) != EGL_TRUE)
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

	util_egl_destroy_default_surface();

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

	*pContext = nullptr;
}

void* getExtensionsFunction(const char* pExtFunctionName)
{
	void *p = (void*)eglGetProcAddress(pExtFunctionName);
	if (!p)
	{
		DLOGF(LogLevel::eWARNING, "GLES extension function \"%s\" not available for this device", pExtFunctionName);
		return nullptr;
	}

	return p;
}

bool addGLSurface(GLContext context, GLConfig config, const WindowHandle* pWindowHandle, GLSurface* pOutSurface)
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

	util_egl_destroy_default_surface();

	util_egl_get_surface_size(*pOutSurface);

	return true;
}

void removeGLSurface(GLContext context, GLConfig config, GLSurface* pSurface)
{
	ASSERT(pSurface);

	GLSurface defaultSurface = util_egl_get_default_surface(config);
	if (eglMakeCurrent(gDisplay, defaultSurface, defaultSurface, context) != EGL_TRUE)
	{
		EGLint error = eglGetError();
		LOGF(LogLevel::eERROR, "Failed to bind default EGL surface! Error {%d}", error);
	}

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