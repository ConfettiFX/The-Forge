
#include <gainput/gainput.h>

#if defined(GAINPUT_PLATFORM_WIN)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdio.h>
#define LOG(...) {char buf[256]; sprintf(buf, __VA_ARGS__); OutputDebugStringA(buf); }


// Define your user buttons
enum Button
{
	ButtonMenu,
	ButtonConfirm,
	MouseX,
	MouseY
};


static char szWindowClass[] = "win32app";
const char* windowName = "Gainput basic sample";
const int width = 800;
const int height = 600;
bool doExit = false;


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	char greeting[] = "Hello, World!";

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
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
		MessageBox(NULL, "Call to RegisterClassEx failed!", "Gainput basic sample", NULL);
		return 1;
	}

	HWND hWnd = CreateWindow(
			szWindowClass,
			windowName,
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
		MessageBox(NULL, "Call to CreateWindow failed!", "Gainput basic sample", NULL);
		return 1;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// Setup Gainput
	gainput::InputManager manager;
	manager.SetDisplaySize(width, height);
	gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();
	gainput::DeviceId keyboardId = manager.CreateDevice<gainput::InputDeviceKeyboard>();
	gainput::DeviceId padId = manager.CreateDevice<gainput::InputDevicePad>();

	gainput::InputMap map(manager);
	map.MapBool(ButtonMenu, keyboardId, gainput::KeyEscape);
	map.MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);
	map.MapFloat(MouseX, mouseId, gainput::MouseAxisX);
	map.MapFloat(MouseY, mouseId, gainput::MouseAxisY);
	map.MapBool(ButtonConfirm, padId, gainput::PadButtonA);

	while (!doExit)
	{
		// Update Gainput
		manager.Update();

		MSG msg;
		while (PeekMessage(&msg, hWnd,  0, 0, PM_REMOVE)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			// Forward any input messages to Gainput
			manager.HandleMessage(msg);
		}

		// Check button states
		if (map.GetBoolWasDown(ButtonConfirm))
		{
			gainput::InputDevicePad* pad = static_cast<gainput::InputDevicePad*>(manager.GetDevice(padId));
			pad->Vibrate(1.0f, 0.0f);
		}
		if (map.GetBoolWasDown(ButtonMenu))
		{
			gainput::InputDevicePad* pad = static_cast<gainput::InputDevicePad*>(manager.GetDevice(padId));
			pad->Vibrate(0.0f, 0.0f);
		}

		if (map.GetBoolWasDown(ButtonMenu))
		{
			LOG("Open Menu!!\n");
		}
		if (map.GetBoolWasDown(ButtonConfirm))
		{
			LOG("Confirmed!!\n");
		}

		if (map.GetFloatDelta(MouseX) != 0.0f || map.GetFloatDelta(MouseY) != 0.0f)
		{
			LOG("Mouse: %f, %f\n", map.GetFloat(MouseX), map.GetFloat(MouseY));
		}
	}

	return 0;
}
#endif

