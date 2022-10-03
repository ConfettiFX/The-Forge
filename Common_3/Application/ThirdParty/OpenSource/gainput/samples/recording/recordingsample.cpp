
#include <gainput/gainput.h>

#include "../samplefw/SampleFramework.h"


enum Button
{
	ButtonStartRecording,
	ButtonStopRecording,
	ButtonStartPlaying,
	ButtonStopPlaying,
	ButtonSerialize,
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
	SfwOpenWindow("Gainput: Listener sample");

	gainput::InputManager manager;

	const gainput::DeviceId keyboardId = manager.CreateDevice<gainput::InputDeviceKeyboard>();
	const gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();
	manager.CreateDevice<gainput::InputDevicePad>();
	manager.CreateDevice<gainput::InputDeviceTouch>();

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_WIN)
	manager.SetDisplaySize(SfwGetWidth(), SfwGetHeight());
#endif

	SfwSetInputManager(&manager);

	gainput::InputMap map(manager, "testmap");

	map.MapBool(ButtonStartRecording, keyboardId, gainput::KeyF1);
	map.MapBool(ButtonStopRecording, keyboardId, gainput::KeyF2);
	map.MapBool(ButtonStartPlaying, keyboardId, gainput::KeyF3);
	map.MapBool(ButtonStopPlaying, keyboardId, gainput::KeyF4);
	map.MapBool(ButtonSerialize, keyboardId, gainput::KeyF5);

	MyDeviceButtonListener myDeviceButtonListener(manager);
	manager.AddListener(&myDeviceButtonListener);

	gainput::InputRecorder inputRecorder(manager);
	inputRecorder.AddDeviceToRecord(mouseId);
	gainput::InputPlayer inputPlayer(manager);

	gainput::InputRecording* serializedRecording = 0;

	bool doExit = false;

	while (!SfwIsDone() && !doExit)
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

		if (map.GetBoolWasDown(ButtonStartRecording))
		{
			SFW_LOG("Started recording\n");
			inputRecorder.Start();
		}
		else if (map.GetBoolWasDown(ButtonStopRecording))
		{
			SFW_LOG("Stopped recording\n");
			inputRecorder.Stop();
		}
		else if (map.GetBoolWasDown(ButtonStartPlaying))
		{
			SFW_LOG("Started playing\n");
			inputPlayer.SetRecording(inputRecorder.GetRecording());
			inputPlayer.Start();
		}
		else if (map.GetBoolWasDown(ButtonStopPlaying))
		{
			SFW_LOG("Stopped playing\n");
			inputPlayer.Stop();
		}
		else if (map.GetBoolWasDown(ButtonSerialize))
		{
			inputPlayer.Stop();

			SFW_LOG("Serializing recording\n");
			const size_t dataSize = inputRecorder.GetRecording()->GetSerializedSize();
			SFW_LOG("Size: %lu\n", dataSize);
			void* serializedRecordingData = malloc(dataSize);
			inputRecorder.GetRecording()->GetSerialized(manager, serializedRecordingData);

			SFW_LOG("Deserialzing recording\n");
			if (serializedRecording)
			{
				delete serializedRecording;
			}
			serializedRecording = new gainput::InputRecording(manager, serializedRecordingData, dataSize);

			SFW_LOG("Playing deserialized recording\n");
			inputPlayer.SetRecording(serializedRecording);
			inputPlayer.Start();

			free(serializedRecordingData);
		}
	}

	if (serializedRecording)
	{
		delete serializedRecording;
	}

	SfwCloseWindow();
}

