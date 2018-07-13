
#include <gainput/gainput.h>

#if defined(GAINPUT_PLATFORM_LINUX)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <iostream>


// Define your user buttons
enum Button
{
	ButtonMenu,
	ButtonConfirm,
	MouseX,
	MouseY
};


const char* windowName = "Gainput basic sample";
const int width = 800;
const int height = 600;


int main(int argc, char** argv)
{
	static int attributeListDbl[] = {      GLX_RGBA,      GLX_DOUBLEBUFFER, /*In case single buffering is not supported*/      GLX_RED_SIZE,   1,      GLX_GREEN_SIZE, 1,      GLX_BLUE_SIZE,  1,
		None };

	Display* xDisplay = XOpenDisplay(0);
	if (xDisplay == 0)
	{
		std::cerr << "Cannot connect to X server." << std::endl;
		return -1;
	}

	Window root = DefaultRootWindow(xDisplay);

	XVisualInfo* vi = glXChooseVisual(xDisplay, DefaultScreen(xDisplay), attributeListDbl);
	assert(vi);

	GLXContext context = glXCreateContext(xDisplay, vi, 0, GL_TRUE);

	Colormap cmap = XCreateColormap(xDisplay, root,                   vi->visual, AllocNone);

	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask = ExposureMask
		| KeyPressMask | KeyReleaseMask
		| PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

	Window xWindow = XCreateWindow(
			xDisplay, root,
			0, 0, width, height, 0,
			CopyFromParent, InputOutput,
			CopyFromParent, CWEventMask,
			&swa
			);

	glXMakeCurrent(xDisplay, xWindow, context);

	XSetWindowAttributes xattr;
	xattr.override_redirect = False;
	XChangeWindowAttributes(xDisplay, xWindow, CWOverrideRedirect, &xattr);

	XMapWindow(xDisplay, xWindow);
	XStoreName(xDisplay, xWindow, windowName);

	// Setup Gainput
	gainput::InputManager manager;
	const gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();
	const gainput::DeviceId keyboardId = manager.CreateDevice<gainput::InputDeviceKeyboard>();
	const gainput::DeviceId padId = manager.CreateDevice<gainput::InputDevicePad>();

	gainput::InputMap map(manager);
	map.MapBool(ButtonMenu, keyboardId, gainput::KeyEscape);
	map.MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);
	map.MapFloat(MouseX, mouseId, gainput::MouseAxisX);
	map.MapFloat(MouseY, mouseId, gainput::MouseAxisY);
	map.MapBool(ButtonConfirm, padId, gainput::PadButtonA);

	manager.SetDisplaySize(width, height);

	for (;;)
	{
		// Update Gainput
		manager.Update();

		XEvent event;
		while (XPending(xDisplay))
		{
			XNextEvent(xDisplay, &event);
			manager.HandleEvent(event);
		}

		// Check button states
		if (map.GetBoolWasDown(ButtonMenu))
		{
			std::cout << "Open menu!!" << std::endl;
		}
		if (map.GetBoolWasDown(ButtonConfirm))
		{
			std::cout << "Confirmed!!" << std::endl;
		}

		if (map.GetFloatDelta(MouseX) != 0.0f || map.GetFloatDelta(MouseY) != 0.0f)
		{
			std::cout << "Mouse: " << map.GetFloat(MouseX) << ", " << map.GetFloat(MouseY) << std::endl;
		}
	}

	XDestroyWindow(xDisplay, xWindow);
	XCloseDisplay(xDisplay);

	return 0;
}
#endif

