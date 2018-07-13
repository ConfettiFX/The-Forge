
#include <gainput/gainput.h>

#include "../samplefw/SampleFramework.h"

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_WIN)
#define ENABLE_FILEIO
#include <iostream>
#include <fstream>
#include <sstream>
#endif


enum Button
{
	ButtonReset,
	ButtonLoad,
	ButtonSave,
	ButtonTest,
	ButtonMouseX,
	ButtonHttp,
	ButtonCount
};


void SaveInputMap(const gainput::InputMap& map)
{
	SFW_LOG("Saving: %s...\n", map.GetName());
	const gainput::InputManager& manager = map.GetManager();
#ifdef ENABLE_FILEIO
	std::ofstream of;
	of.open("mappings.txt");
#endif
	const size_t maxCount = 32;
	gainput::DeviceButtonSpec buttons[maxCount];
	for (int i = ButtonReset; i < ButtonCount; ++i)
	{
		if (!map.IsMapped(i))
			continue;
		const size_t count = map.GetMappings(gainput::UserButtonId(i), buttons, maxCount);
		for (size_t b = 0; b < count; ++b)
		{
			const gainput::InputDevice* device = manager.GetDevice(buttons[b].deviceId);
			char name[maxCount];
			device->GetButtonName(buttons[b].buttonId, name, maxCount);
			const gainput::ButtonType buttonType = device->GetButtonType(buttons[b].buttonId);
			SFW_LOG("Btn %d: %d:%d (%s%d:%s) type=%d\n", i, buttons[b].deviceId, buttons[b].buttonId, device->GetTypeName(), device->GetIndex(), name, buttonType);
#ifdef ENABLE_FILEIO
			of << i << " " << device->GetTypeName() << " " << device->GetIndex() << " " << name << " " << buttonType << std::endl;
#endif
		}
	}
#ifdef ENABLE_FILEIO
	of.close();
#endif
}

void LoadInputMap(gainput::InputMap& map)
{
	SFW_LOG("Loading...\n");
#ifdef ENABLE_FILEIO
	const gainput::InputManager& manager = map.GetManager();
	std::ifstream ifs("mappings.txt");
	if(ifs.is_open())
	{
		map.Clear();
		std::string line;
		int i;
		std::string typeName;
		unsigned index;
		std::string name;
		int buttonType;
		while (ifs.good())
		{
			getline(ifs, line);
			if (!ifs.good())
				break;
			std::istringstream iss(line);
			iss >> i;
			iss >> typeName;
			iss >> index;
			iss >> name;
			iss >> buttonType;
			gainput::DeviceId deviceId = manager.FindDeviceId(typeName.c_str(), index);
			const gainput::InputDevice* device = manager.GetDevice(deviceId);
			gainput::DeviceButtonId button = device->GetButtonByName(name.c_str());
			SFW_LOG("Btn %d: %d:%d (%s%d:%s) type=%d\n", i, deviceId, button, typeName.c_str(), index, name.c_str(), buttonType);
			if (buttonType == gainput::BT_BOOL)
			{
				map.MapBool(i, deviceId, button);
			}
			else if (buttonType == gainput::BT_FLOAT)
			{
				map.MapFloat(i, deviceId, button);
			}
		}
		ifs.close();
	}
#endif
}


void SampleMain()
{
	SfwOpenWindow("Gainput: Dynamic sample");

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
	map.MapBool(ButtonReset, keyboardId, gainput::KeyEscape);
	map.MapBool(ButtonSave, keyboardId, gainput::KeyF1);
	map.MapBool(ButtonLoad, keyboardId, gainput::KeyF2);
	map.MapBool(ButtonHttp, keyboardId, gainput::KeyF3);
	map.MapFloat(ButtonMouseX, mouseId, gainput::MouseAxisX);

	gainput::DeviceButtonSpec anyButton[32];
	bool mapped = false;

	SFW_LOG("No button mapped, please press any button.\n");
	SFW_LOG("Press ESC to reset.\n");

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

		if (map.GetBoolWasDown(ButtonReset))
		{
			SFW_LOG("Mapping reset. Press any button.\n");
			mapped = false;
			map.Unmap(ButtonTest);
		}

		if (map.GetBoolWasDown(ButtonSave))
		{
			SaveInputMap(map);
		}

		if (map.GetBoolWasDown(ButtonLoad))
		{
			LoadInputMap(map);
		}

		if (map.GetBoolWasDown(ButtonHttp))
		{
			static bool enabled = false;
			enabled = !enabled;
			gainput::DevSetHttp(enabled);
		}


		if (!mapped)
		{
			const size_t anyCount = manager.GetAnyButtonDown(anyButton, 32);
			for (size_t i = 0; i < anyCount; ++i)
			{
				// Filter the returned buttons as needed.
				const gainput::InputDevice* device = manager.GetDevice(anyButton[i].deviceId);
				if (device->GetButtonType(anyButton[i].buttonId) == gainput::BT_BOOL
					&& map.GetUserButtonId(anyButton[i].deviceId, anyButton[i].buttonId) == gainput::InvalidDeviceButtonId)
				{
					SFW_LOG("Mapping to: %d:%d\n", anyButton[i].deviceId, anyButton[i].buttonId);
					map.MapBool(ButtonTest, anyButton[i].deviceId, anyButton[i].buttonId);
					mapped = true;
					break;
				}
			}
		}
		else
		{
			if (map.GetBoolWasDown(ButtonTest))
			{
				SFW_LOG("Button was down!\n");
			}
		}
	}

	SfwCloseWindow();
}


