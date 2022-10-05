
#include <gainput/gainput.h>
#include "SampleFramework.h"

extern void SampleMain();

const int width = 800;
const int height = 600;


int SfwGetWidth()
{
	return width;
}

int SfwGetHeight()
{
	return height;
}


#if defined(GAINPUT_PLATFORM_LINUX)

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>

Display* xDisplay = 0;
Window xWindow;
GLXContext glxContext;


void SfwOpenWindow(const char* title)
{
	static int attributeListDbl[] = {      GLX_RGBA,      GLX_DOUBLEBUFFER, /*In case single buffering is not supported*/      GLX_RED_SIZE,   1,      GLX_GREEN_SIZE, 1,      GLX_BLUE_SIZE,  1,
		None };

	xDisplay = XOpenDisplay(0);
	if (xDisplay == 0)
	{
		SFW_LOG("Cannot connect to X server.\n");
		return;
	}

	Window root = DefaultRootWindow(xDisplay);

	XVisualInfo* vi = glXChooseVisual(xDisplay, DefaultScreen(xDisplay), attributeListDbl);
	assert(vi);

	glxContext = glXCreateContext(xDisplay, vi, 0, GL_TRUE);

	Colormap cmap = XCreateColormap(xDisplay, root,                   vi->visual, AllocNone);

	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask = ExposureMask
		| KeyPressMask | KeyReleaseMask
		| PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

	xWindow = XCreateWindow(
			xDisplay, root,
			0, 0, width, height, 0,
			CopyFromParent, InputOutput,
			CopyFromParent, CWEventMask,
			&swa
			);

	glXMakeCurrent(xDisplay, xWindow, glxContext);

	XSetWindowAttributes xattr;
	xattr.override_redirect = False;
	XChangeWindowAttributes(xDisplay, xWindow, CWOverrideRedirect, &xattr);

	XMapWindow(xDisplay, xWindow);
	XStoreName(xDisplay, xWindow, title);

	Atom wmDelete=XInternAtom(xDisplay, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(xDisplay, xWindow, &wmDelete, 1);

	XFree(vi);
}

void SfwCloseWindow()
{
	glXDestroyContext(xDisplay, glxContext);
	XDestroyWindow(xDisplay, xWindow);
	XCloseDisplay(xDisplay);
}

void SfwUpdate()
{
}

bool SfwIsDone()
{
	return false;
}

void SfwSetInputManager(gainput::InputManager* manager)
{
}

Display* SfwGetXDisplay()
{
	return xDisplay;
}

int main(int argc, char** argv)
{
	SampleMain();
	return 0;
}


#elif defined(GAINPUT_PLATFORM_WIN)

static char szWindowClass[] = "win32app";
bool doExit = false;
HINSTANCE hInstance;
int nCmdShow;
HWND hWnd;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	char greeting[] = "Hello, Gainput!";

	switch (message)
	{
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			TextOut(hdc, 5, 5, greeting, strlen(greeting));
			EndPaint(hWnd, &ps);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			doExit = true;
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	return 0;
}

void SfwOpenWindow(const char* title)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = 0;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = 0;

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, "Call to RegisterClassEx failed!", title, NULL);
		return;
	}

	hWnd = CreateWindow(
			szWindowClass,
			title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			width, height,
			NULL,
			NULL,
			hInstance,
			NULL
			);

	if (!hWnd)
	{
		MessageBox(NULL, "Call to CreateWindow failed!", title, NULL);
		return;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
}

void SfwCloseWindow()
{
}

void SfwUpdate()
{
}

bool SfwIsDone()
{
	return doExit;
}

void SfwSetInputManager(gainput::InputManager* manager)
{
}

HWND SfwGetHWnd()
{
	return hWnd;
}

int WINAPI WinMain(HINSTANCE hInstanceMain, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShowMain)
{
	hInstance = hInstanceMain;
	nCmdShow = nCmdShowMain;
	SampleMain();
	return 0;
}


#elif defined(GAINPUT_PLATFORM_ANDROID)


#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "gainput", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "gainput", __VA_ARGS__))

static bool doExit = false;
struct android_app* state;

static int32_t MyHandleInput(struct android_app* app, AInputEvent* event)
{
	// Forward input events to Gainput
	gainput::InputManager* inputManager = (gainput::InputManager*)app->userData;
	static bool resSet = false;
	if (!resSet)
	{
		inputManager->SetDisplaySize(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
		resSet = true;
	}
	return inputManager->HandleInput(event);
}

static void MyHandleCmd(struct android_app* app, int32_t cmd)
{
	switch (cmd)
	{
		case APP_CMD_SAVE_STATE:
			break;
		case APP_CMD_INIT_WINDOW:
			break;
		case APP_CMD_TERM_WINDOW:
			doExit = true;
			break;
		case APP_CMD_LOST_FOCUS:
			doExit = true;
			break;
		case APP_CMD_GAINED_FOCUS:
			// bring back a certain functionality, like monitoring the accelerometer
			break;
	}
}

void SfwOpenWindow(const char* title)
{
	app_dummy();
	LOGI("Opening window: %s\n", title);
}

void SfwCloseWindow()
{
	ANativeActivity_finish(state->activity);
	LOGI("Closing window\n");
}

void SfwUpdate()
{
	int ident;
	int events;
	struct android_poll_source* source;

	while (!doExit && (ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0)
	{
		if (source != NULL)
		{
			source->process(state, source);
		}

		if (state->destroyRequested != 0)
		{
			doExit = true;
		}
	}
}

bool SfwIsDone()
{
	return doExit;
}

void SfwSetInputManager(gainput::InputManager* manager)
{
	state->userData = manager;
	state->onInputEvent = &MyHandleInput;
	state->onAppCmd = &MyHandleCmd;
}

void android_main(struct android_app* stateMain)
{
	state = stateMain;

	SampleMain();
}

#elif defined(GAINPUT_PLATFORM_MAC)

void SfwOpenWindow(const char* title)
{
}

void SfwCloseWindow()
{
}

void SfwUpdate()
{
}

bool SfwIsDone()
{
	return false;
}

void SfwSetInputManager(gainput::InputManager* manager)
{
}

int main(int argc, char** argv)
{
	SampleMain();
	return 0;
}



#endif

