
#include <gainput/gainput.h>

#include "../samplefw/SampleFramework.h"


enum Button
{
	ButtonClient,
	ButtonTest,
	ButtonMouseX,
	ButtonMouseLeft,
	ButtonCount
};

class MyDeviceButtonListener : public gainput::InputListener
{
public:
	MyDeviceButtonListener(gainput::InputManager& manager) : manager(manager) { }

	bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
	{
		const gainput::InputDevice* device = manager.GetDevice(deviceId);
		char buttonName[64];
		device->GetButtonName(deviceButton, buttonName, 64);
		SFW_LOG("Device %d (%s%d) bool button (%d/%s) changed: %d -> %d\n", deviceId, device->GetTypeName(), device->GetIndex(), deviceButton, buttonName, oldValue, newValue);
		return true;
	}

	bool OnDeviceButtonFloat(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
	{
		const gainput::InputDevice* device = manager.GetDevice(deviceId);
		char buttonName[64];
		device->GetButtonName(deviceButton, buttonName, 64);
		SFW_LOG("Device %d (%s%d) float button (%d/%s) changed: %f -> %f\n", deviceId, device->GetTypeName(), device->GetIndex(), deviceButton, buttonName, oldValue, newValue);
		return true;
	}

private:
	gainput::InputManager& manager;
};

void SampleMain()
{
	SfwOpenWindow("Gainput: Sync sample");

	gainput::InputManager manager;

	const gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();
	manager.CreateDevice<gainput::InputDeviceTouch>();
	const gainput::DeviceId keyboardId = manager.CreateDevice<gainput::InputDeviceKeyboard>();
	manager.CreateDevice<gainput::InputDevicePad>();

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_WIN)
	manager.SetDisplaySize(SfwGetWidth(), SfwGetHeight());
#endif

	SfwSetInputManager(&manager);

	gainput::InputMap map(manager, "testmap");
	map.MapBool(ButtonClient, keyboardId, gainput::KeyF1);
	map.MapFloat(ButtonMouseX, mouseId, gainput::MouseAxisX);
	map.MapFloat(ButtonMouseLeft, mouseId, gainput::MouseButtonLeft);

	MyDeviceButtonListener myDeviceButtonListener(manager);
	manager.AddListener(&myDeviceButtonListener);

	SFW_LOG("Press F1 to connect to localhost and start syncing.\n");

	bool doExit = false;

	while (!SfwIsDone() & !doExit)
	{
		manager.Update();

#if defined(GAINPUT_PLATFORM_LINUX)
		XEvent event;
		while (XPending(SfwGetXDisplay()))
		{
			XNextEvent(SfwGetXDisplay(), &event);
			manager.HandleEvent(event);
			if (event.type == DestroyNotify || event.type == ClientMessage)
			{
				doExit = true;
			}
		}
#elif defined(GAINPUT_PLATFORM_WIN)
		MSG msg;
		while (PeekMessage(&msg, SfwGetHWnd(),  0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			manager.HandleMessage(msg);
		}
#endif

		SfwUpdate();

		if (map.GetBoolWasDown(ButtonClient))
		{
			manager.ConnectForStateSync("127.0.0.1", 1211);
			manager.StartDeviceStateSync(mouseId);
		}

		if (map.GetBoolWasDown(ButtonMouseLeft))
		{
			SFW_LOG("Mouse was down.\n");
		}

		if (map.GetFloatDelta(ButtonMouseX) != 0.0f)
		{
			SFW_LOG("Mouse X: %f\n", map.GetFloat(ButtonMouseX));
		}

	}

	SfwCloseWindow();
}


