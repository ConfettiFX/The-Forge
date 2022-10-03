
#include "catch.hpp"

#include <gainput/gainput.h>

using namespace gainput;

TEST_CASE("InputManager/CreateDevice", "")
{
	InputManager manager;

	const DeviceId deviceId = manager.CreateDevice<InputDeviceKeyboard>();
	REQUIRE(deviceId != InvalidDeviceId);

	InputDevice* device = manager.GetDevice(deviceId);
	REQUIRE(device);
	REQUIRE(device->GetType() == InputDevice::DT_KEYBOARD);
	REQUIRE(device->GetIndex() == 0);

	const DeviceId deviceId2 = manager.CreateDevice<InputDeviceMouse>();
	REQUIRE(deviceId2 != InvalidDeviceId);

	InputDevice* device2 = manager.GetDevice(deviceId2);
	REQUIRE(device2);
	REQUIRE(device2->GetType() == InputDevice::DT_MOUSE);
	REQUIRE(device2->GetIndex() == 0);

	InputDevicePad* device3 = manager.CreateAndGetDevice<InputDevicePad>();
	REQUIRE(device3);
	REQUIRE(device3->GetType() == InputDevice::DT_PAD);
	REQUIRE(device3->GetIndex() == 0);
	REQUIRE(device3->GetDeviceId() != InvalidDeviceId);
}

TEST_CASE("InputManager/FindDeviceId", "")
{
	InputManager manager;
	const DeviceId deviceId = manager.CreateDevice<InputDeviceKeyboard>();
	const InputDevice* device = manager.GetDevice(deviceId);
	const DeviceId deviceId2 = manager.CreateDevice<InputDeviceMouse>();
	const InputDevice* device2 = manager.GetDevice(deviceId2);
	const DeviceId deviceId3 = manager.CreateDevice<InputDevicePad>();
	const InputDevice* device3 = manager.GetDevice(deviceId3);

	REQUIRE(manager.FindDeviceId(device->GetTypeName(), device->GetIndex()) == deviceId);
	REQUIRE(manager.FindDeviceId(device->GetType(), device->GetIndex()) == deviceId);

	REQUIRE(manager.FindDeviceId(device2->GetTypeName(), device2->GetIndex()) == deviceId2);
	REQUIRE(manager.FindDeviceId(device2->GetType(), device2->GetIndex()) == deviceId2);

	REQUIRE(manager.FindDeviceId(device3->GetTypeName(), device3->GetIndex()) == deviceId3);
	REQUIRE(manager.FindDeviceId(device3->GetType(), device3->GetIndex()) == deviceId3);
}

TEST_CASE("InputManager/GetDeviceCountByType", "")
{
	InputManager manager;

	for (int type = InputDevice::DT_MOUSE; type < InputDevice::DT_COUNT; ++type)
	{
		REQUIRE(manager.GetDeviceCountByType(InputDevice::DeviceType(type)) == 0);
	}

	manager.CreateDevice<InputDeviceKeyboard>();
	manager.CreateDevice<InputDeviceMouse>();
	manager.CreateDevice<InputDevicePad>();

	REQUIRE(manager.GetDeviceCountByType(InputDevice::DT_KEYBOARD) == 1);
	REQUIRE(manager.GetDeviceCountByType(InputDevice::DT_MOUSE) == 1);
	REQUIRE(manager.GetDeviceCountByType(InputDevice::DT_PAD) == 1);
	REQUIRE(manager.GetDeviceCountByType(InputDevice::DT_TOUCH) == 0);
}

TEST_CASE("InputManager/GetAnyButtonDown", "")
{
	InputManager manager;
	REQUIRE(manager.GetAnyButtonDown(0, 0) == 0);
}

