
#include "catch.hpp"

#include <gainput/gainput.h>

using namespace gainput;

enum TestButtons
{
	ButtonA,
	ButtonStart = ButtonA,
	ButtonB,
	ButtonC,
	ButtonD,
	ButtonE,
	ButtonF,
	ButtonG,
	ButtonCount
};

TEST_CASE("InputMap/create", "")
{
	InputManager manager;
	InputMap map(manager, "testmap");

	REQUIRE(&manager == &map.GetManager());
	REQUIRE(map.GetName());
	REQUIRE(strcmp(map.GetName(), "testmap") == 0);

	for (int b = ButtonStart; b < ButtonCount; ++b)
	{
		REQUIRE(!map.IsMapped(b));
	}

	InputMap map2(manager);
	REQUIRE(!map2.GetName());
}

TEST_CASE("InputMap/map_bool", "")
{
	InputManager manager;
	const DeviceId keyboardId = manager.CreateDevice<InputDeviceKeyboard>();
	const DeviceId mouseId = manager.CreateDevice<InputDeviceMouse>();
	InputMap map(manager, "testmap");

	REQUIRE(map.MapBool(ButtonA, keyboardId, KeyA));
	REQUIRE(map.MapBool(ButtonA, keyboardId, KeyB));
	REQUIRE(map.MapBool(ButtonA, keyboardId, KeyEscape));
	REQUIRE(map.MapBool(ButtonA, mouseId, MouseButtonLeft));
	REQUIRE(map.IsMapped(ButtonA));

	REQUIRE(map.MapBool(ButtonB, keyboardId, KeyF2));
	REQUIRE(map.MapBool(ButtonB, mouseId, MouseButtonLeft));
	REQUIRE(map.IsMapped(ButtonB));

	map.Clear();
	for (int b = ButtonStart; b < ButtonCount; ++b)
	{
		REQUIRE(!map.IsMapped(b));
	}

	REQUIRE(map.MapBool(ButtonA, mouseId, MouseButtonRight));
	REQUIRE(map.IsMapped(ButtonA));

	map.Unmap(ButtonA);
	REQUIRE(!map.IsMapped(ButtonA));

	DeviceButtonSpec mappings[32];
	REQUIRE(map.GetMappings(ButtonA, mappings, 32) == 0);

	REQUIRE(map.MapBool(ButtonA, mouseId, MouseButtonMiddle));
	REQUIRE(map.MapBool(ButtonA, keyboardId, KeyD));
	REQUIRE(map.MapBool(ButtonD, keyboardId, KeyB));

	REQUIRE(map.GetMappings(ButtonA, mappings, 32) == 2);
	REQUIRE(mappings[0].deviceId == mouseId);
	REQUIRE(mappings[0].buttonId == MouseButtonMiddle);
	REQUIRE(mappings[1].deviceId == keyboardId);
	REQUIRE(mappings[1].buttonId == KeyD);

	char buf[32];
	REQUIRE(map.GetUserButtonName(ButtonA, buf, 32));

	REQUIRE(map.GetUserButtonId(mouseId, MouseButtonMiddle) == ButtonA);
	REQUIRE(map.GetUserButtonId(keyboardId, KeyD) == ButtonA);
	REQUIRE(map.GetUserButtonId(keyboardId, KeyB) == ButtonD);
}

TEST_CASE("InputMap/map_float", "")
{
	InputManager manager;
	const DeviceId keyboardId = manager.CreateDevice<InputDeviceKeyboard>();
	const DeviceId mouseId = manager.CreateDevice<InputDeviceMouse>();
	const DeviceId padId = manager.CreateDevice<InputDevicePad>();
	InputMap map(manager, "testmap");

	REQUIRE(map.MapFloat(ButtonA, keyboardId, KeyA));
	REQUIRE(map.MapFloat(ButtonA, keyboardId, KeyB));
	REQUIRE(map.MapFloat(ButtonA, keyboardId, KeyEscape));
	REQUIRE(map.MapFloat(ButtonA, mouseId, MouseButtonLeft));
	REQUIRE(map.MapFloat(ButtonA, mouseId, MouseAxisY));
	REQUIRE(map.MapFloat(ButtonA, padId, PadButtonLeftStickX));
	REQUIRE(map.IsMapped(ButtonA));

	REQUIRE(map.MapFloat(ButtonB, keyboardId, KeyF2));
	REQUIRE(map.MapFloat(ButtonB, mouseId, MouseAxisX));
	REQUIRE(map.IsMapped(ButtonB));

	map.Clear();
	for (int b = ButtonStart; b < ButtonCount; ++b)
	{
		REQUIRE(!map.IsMapped(b));
	}

	REQUIRE(map.MapFloat(ButtonA, mouseId, MouseAxisX));
	REQUIRE(map.IsMapped(ButtonA));

	map.Unmap(ButtonA);
	REQUIRE(!map.IsMapped(ButtonA));

	DeviceButtonSpec mappings[32];
	REQUIRE(map.GetMappings(ButtonA, mappings, 32) == 0);

	REQUIRE(map.MapFloat(ButtonA, mouseId, MouseAxisX));
	REQUIRE(map.MapFloat(ButtonA, keyboardId, KeyF5));
	REQUIRE(map.MapFloat(ButtonD, padId, PadButtonLeftStickY));

	REQUIRE(map.GetMappings(ButtonA, mappings, 32) == 2);
	REQUIRE(mappings[0].deviceId == mouseId);
	REQUIRE(mappings[0].buttonId == MouseAxisX);
	REQUIRE(mappings[1].deviceId == keyboardId);
	REQUIRE(mappings[1].buttonId == KeyF5);

	char buf[32];
	REQUIRE(map.GetUserButtonName(ButtonA, buf, 32));

	REQUIRE(map.GetUserButtonId(mouseId, MouseAxisX) == ButtonA);
	REQUIRE(map.GetUserButtonId(keyboardId, KeyF5) == ButtonA);
	REQUIRE(map.GetUserButtonId(padId, PadButtonLeftStickY) == ButtonD);
}

TEST_CASE("InputMap/SetDeadZone_SetUserButtonPolicy", "")
{
	InputManager manager;
	const DeviceId keyboardId = manager.CreateDevice<InputDeviceKeyboard>();
	const DeviceId mouseId = manager.CreateDevice<InputDeviceMouse>();
	const DeviceId padId = manager.CreateDevice<InputDevicePad>();
	InputMap map(manager, "testmap");

	for (int b = ButtonStart; b < ButtonCount; ++b)
	{
		REQUIRE(!map.SetDeadZone(b, 0.1f));
		REQUIRE(!map.SetUserButtonPolicy(b, InputMap::UBP_AVERAGE));
	}

	REQUIRE(map.MapFloat(ButtonA, mouseId, MouseAxisX));

	REQUIRE(map.SetDeadZone(ButtonA, 0.01f));
	REQUIRE(map.SetDeadZone(ButtonA, 0.0f));
	REQUIRE(map.SetDeadZone(ButtonA, 1.01f));
	REQUIRE(!map.SetDeadZone(ButtonF, 1.01f));

	REQUIRE(map.SetUserButtonPolicy(ButtonA, InputMap::UBP_AVERAGE));
	REQUIRE(map.SetUserButtonPolicy(ButtonA, InputMap::UBP_MAX));
	REQUIRE(map.SetUserButtonPolicy(ButtonA, InputMap::UBP_MIN));
	REQUIRE(map.SetUserButtonPolicy(ButtonA, InputMap::UBP_FIRST_DOWN));

	map.Clear();

	for (int b = ButtonStart; b < ButtonCount; ++b)
	{
		REQUIRE(!map.SetDeadZone(b, 0.1f));
		REQUIRE(!map.SetUserButtonPolicy(b, InputMap::UBP_AVERAGE));
	}
}


