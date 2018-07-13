
#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_DEV

#include <stdio.h>

#if defined(GAINPUT_PLATFORM_ANDROID)
#include <android/log.h>
#endif

#include "GainputDev.h"
#include "GainputNetAddress.h"
#include "GainputNetListener.h"
#include "GainputNetConnection.h"
#include "GainputMemoryStream.h"
#include "GainputInputDeltaState.h"
#include "GainputHelpers.h"
#include "GainputLog.h"

#if _MSC_VER
#define snprintf _snprintf
#endif


namespace gainput
{

static NetListener* devListener = 0;
static NetConnection* devConnection = 0;
static InputManager* inputManager = 0;
static Allocator* allocator = 0;
static Array<const InputMap*> devMaps(GetDefaultAllocator());
static Array<const InputDevice*> devDevices(GetDefaultAllocator());
static bool devSendInfos = false;
static Array<DeviceId> devSyncedDevices(GetDefaultAllocator());
static size_t devFrame = 0;
static bool useHttp = false;

static const unsigned kMaxReadTries = 1000000;

static size_t SendMessage(Stream& stream)
{
	const uint8_t length = stream.GetSize();
	GAINPUT_ASSERT(length > 0);
	size_t sent = devConnection->Send(&length, sizeof(length));
	sent += devConnection->Send(stream);
	return sent;
}

static bool ReadMessage(Stream& stream, bool& outFailed)
{
	stream.Reset();
	size_t received = devConnection->Receive(stream, 1);
	if (!received)
		return false;
	uint8_t length;
	stream.Read(length);
	stream.Reset();
	received = 0;
    unsigned tries = 0;
    bool failed = false;
	while (received < length)
	{
		stream.SeekEnd(0);
		received += devConnection->Receive(stream, length - received);
        ++tries;
        if (tries >= kMaxReadTries)
        {
            failed = true;
            break;
        }
	}
    outFailed = failed;
    if (failed)
    {
        GAINPUT_LOG("GAINPUT: ReadMessage failed.\n");
        return false;
    }
	stream.SeekBegin(0);
	return true;
}

class DevUserButtonListener : public MappedInputListener
{
public:
	DevUserButtonListener(const InputMap* map) : map_(map) { }

	bool OnUserButtonBool(UserButtonId userButton, bool, bool newValue)
	{
		if (!devConnection || !devSendInfos)
		{
			return true;
		}

		Stream* stream = allocator->New<MemoryStream>(32, *allocator);
		stream->Write(uint8_t(DevCmdUserButtonChanged));
		stream->Write(uint32_t(map_->GetId()));
		stream->Write(uint32_t(userButton));
		stream->Write(uint8_t(0));
		stream->Write(uint8_t(newValue));
		stream->SeekBegin(0);
		SendMessage(*stream);
		allocator->Delete(stream);

		return true;
	}

	bool OnUserButtonFloat(UserButtonId userButton, float, float newValue)
	{
		if (!devConnection || !devSendInfos)
		{
			return true;
		}

		Stream* stream = allocator->New<MemoryStream>(32, *allocator);
		stream->Write(uint8_t(DevCmdUserButtonChanged));
		stream->Write(uint32_t(map_->GetId()));
		stream->Write(uint32_t(userButton));
		stream->Write(uint8_t(1));
		stream->Write(newValue);
		stream->SeekBegin(0);
		SendMessage(*stream);
		allocator->Delete(stream);

		return true;
	}

private:
	const InputMap* map_;
};

class DevDeviceButtonListener : public InputListener
{
public:
	DevDeviceButtonListener(const InputManager* inputManager) : inputManager_(inputManager) { }

	bool OnDeviceButtonBool(DeviceId deviceId, DeviceButtonId deviceButton, bool, bool newValue)
	{
		if (!devConnection || devSyncedDevices.find(deviceId) == devSyncedDevices.end())
			return true;
		const InputDevice* device = inputManager_->GetDevice(deviceId);
		GAINPUT_ASSERT(device);
		Stream* stream = allocator->New<MemoryStream>(32, *allocator);
		stream->Write(uint8_t(DevCmdSetDeviceButton));
		stream->Write(uint8_t(device->GetType()));
		stream->Write(uint8_t(device->GetIndex()));
		stream->Write(uint32_t(deviceButton));
		stream->Write(uint8_t(newValue));
		stream->SeekBegin(0);
		SendMessage(*stream);
		allocator->Delete(stream);
		return true;
	}

	bool OnDeviceButtonFloat(DeviceId deviceId, DeviceButtonId deviceButton, float, float newValue)
	{
		if (!devConnection || devSyncedDevices.find(deviceId) == devSyncedDevices.end())
			return true;
		const InputDevice* device = inputManager_->GetDevice(deviceId);
		GAINPUT_ASSERT(device);
		Stream* stream = allocator->New<MemoryStream>(32, *allocator);
		stream->Write(uint8_t(DevCmdSetDeviceButton));
		stream->Write(uint8_t(device->GetType()));
		stream->Write(uint8_t(device->GetIndex()));
		stream->Write(uint32_t(deviceButton));
		stream->Write(newValue);
		stream->SeekBegin(0);
		SendMessage(*stream);
		allocator->Delete(stream);
		return true;
	}

private:
	const InputManager* inputManager_;
};

static HashMap<const InputMap*, DevUserButtonListener*> devUserButtonListeners(GetDefaultAllocator());
DevDeviceButtonListener* devDeviceButtonListener = 0;
ListenerId devDeviceButtonListenerId;



void
SendDevice(const InputDevice* device, Stream* stream, NetConnection*)
{
	const DeviceId deviceId = device->GetDeviceId();
	stream->Reset();
	stream->Write(uint8_t(DevCmdDevice));
	stream->Write(uint32_t(device->GetDeviceId()));
	char deviceName[64];
	snprintf(deviceName, 64, "%s%d", device->GetTypeName(), device->GetIndex());
	stream->Write(uint8_t(strlen(deviceName)));
	stream->Write(deviceName, strlen(deviceName));
	stream->SeekBegin(0);
	SendMessage(*stream);

	for (DeviceButtonId buttonId = 0; buttonId < device->GetInputState()->GetButtonCount(); ++buttonId)
	{
		if (device->IsValidButtonId(buttonId))
		{
			stream->Reset();
			stream->Write(uint8_t(DevCmdDeviceButton));
			stream->Write(uint32_t(deviceId));
			stream->Write(uint32_t(buttonId));
			char buttonName[128];
			const size_t len= device->GetButtonName(buttonId, buttonName, 128);
			stream->Write(uint8_t(len));
			if (len)
				stream->Write(buttonName, len);
			stream->Write(uint8_t(device->GetButtonType(buttonId)));
			stream->SeekBegin(0);
			SendMessage(*stream);
		}
	}
}

void
SendMap(const InputMap* map, Stream* stream, NetConnection*)
{
	stream->Reset();
	stream->Write(uint8_t(DevCmdMap));
	stream->Write(uint32_t(map->GetId()));
	stream->Write(uint8_t(strlen(map->GetName())));
	stream->Write(map->GetName(), strlen(map->GetName()));
	stream->SeekBegin(0);
	SendMessage(*stream);

	DeviceButtonSpec mappings[32];
	for (UserButtonId buttonId = 0; buttonId < 1000; ++buttonId)
	{
		if (map->IsMapped(buttonId))
		{
			const size_t n = map->GetMappings(buttonId, mappings, 32);
			for (size_t i = 0; i < n; ++i)
			{
				stream->Reset();
				stream->Write(uint8_t(DevCmdUserButton));
				stream->Write(uint32_t(map->GetId()));
				stream->Write(uint32_t(buttonId));
				stream->Write(uint32_t(mappings[i].deviceId));
				stream->Write(uint32_t(mappings[i].buttonId));
				stream->Write(map->GetFloat(buttonId));
				stream->SeekBegin(0);
				SendMessage(*stream);
			}
		}
	}
}

void
DevSetHttp(bool enable)
{
	useHttp = enable;
}

void
DevInit(InputManager* manager)
{
	if (devListener)
	{
		return;
	}

#if defined(GAINPUT_PLATFORM_WIN)
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
	{
		GAINPUT_ASSERT(false);
	}
#endif

	inputManager = manager;
	allocator = &manager->GetAllocator();

	NetAddress address("0.0.0.0", 1211);
	devListener = allocator->New<NetListener>(address, *allocator);

    if (devListener->Start(false))
    {
        GAINPUT_LOG("TOOL: Listening...\n");
    }
    else
    {
        GAINPUT_LOG("TOOL: Unable to listen\n");
        allocator->Delete(devListener);
        devListener = 0;
    }

	
}

void
DevShutdown(const InputManager* manager)
{
	if (inputManager != manager)
	{
		return;
	}

	if (devConnection)
	{
		devConnection->Close();
		allocator->Delete(devConnection);
		devConnection = 0;
	}

	if (devListener)
	{
		devListener->Stop();
		allocator->Delete(devListener);
		devListener = 0;
	}

	for (HashMap<const InputMap*, DevUserButtonListener*>::iterator it = devUserButtonListeners.begin();
			it != devUserButtonListeners.end();
			++it)
	{
		allocator->Delete(it->second);
	}

	if (devDeviceButtonListener)
	{
		inputManager->RemoveListener(devDeviceButtonListenerId);
		allocator->Delete(devDeviceButtonListener);
		devDeviceButtonListener = 0;
	}

#if defined(GAINPUT_PLATFORM_WIN)
	WSACleanup();
#endif
}


void
DevUpdate(InputDeltaState* delta)
{
	if (!devConnection && devListener)
	{
		devConnection = devListener->Accept();
		if (!useHttp && devConnection)
		{
			GAINPUT_LOG("GAINPUT: New connection\n");
			Stream* stream = allocator->New<MemoryStream>(1024, *allocator);

			stream->Write(uint8_t(DevCmdHello));
			stream->Write(uint32_t(DevProtocolVersion));
			stream->Write(uint32_t(GetLibVersion()));
			stream->SeekBegin(0);
			SendMessage(*stream);

			allocator->Delete(stream);
		}
	}

	if (!devConnection)
	{
		return;
	}

	if (useHttp)
	{
		Stream* stream = allocator->New<MemoryStream>(128, *allocator);
		stream->Reset();
		size_t received = devConnection->Receive(*stream, 128);
		if (received > 0)
		{
			char* buf = (char*)allocator->Allocate(received+1);
			stream->Read(buf, received);
			buf[received] = 0;
			int touchDevice;
			int id;
			float x;
			float y;
			int down;
			sscanf(buf, "GET /%i/%i/%f/%f/%i HTTP", &touchDevice, &id, &x, &y, &down);
			//GAINPUT_LOG("Touch device #%d state #%d: %f/%f - %d\n", touchDevice, id, x, y, down);
			allocator->Deallocate(buf);

			allocator->Delete(stream);
			devConnection->Close();
			allocator->Delete(devConnection);
			devConnection = 0;

			const DeviceId deviceId = inputManager->FindDeviceId(InputDevice::DT_TOUCH, touchDevice);
			InputDevice* device = inputManager->GetDevice(deviceId);
			GAINPUT_ASSERT(device);
			GAINPUT_ASSERT(device->GetInputState());
			GAINPUT_ASSERT(device->GetPreviousInputState());
			HandleButton(*device, *device->GetInputState(), delta, Touch0Down + id*4, down != 0);
			HandleAxis(*device, *device->GetInputState(), delta, Touch0X + id*4, x);
			HandleAxis(*device, *device->GetInputState(), delta, Touch0Pressure + id*4, float(down)*1.0f);
		}
	}
	else
	{
		Stream* stream = allocator->New<MemoryStream>(1024, *allocator);
        bool readFailed = false;
		while (ReadMessage(*stream, readFailed))
		{
			uint8_t cmd;
			stream->Read(cmd);

			if (cmd == DevCmdGetAllInfos)
			{
				devSendInfos = true;

				int count = 0;
				// Send existing devices
				for (InputManager::const_iterator it = inputManager->begin();
						it != inputManager->end();
						++it)
				{
					const InputDevice* device = it->second;
					SendDevice(device, stream, devConnection);
					++count;
				}
				GAINPUT_LOG("GAINPUT: Sent %d devices.\n", count);

				// Send existing maps
				count = 0;
				for (Array<const InputMap*>::const_iterator it = devMaps.begin();
						it != devMaps.end();
						++it)
				{
					const InputMap* map = *it;
					SendMap(map, stream, devConnection);
					++count;
				}
				GAINPUT_LOG("GAINPUT: Sent %d maps.\n", count);
			}
			else if (cmd == DevCmdStartDeviceSync)
			{
				uint8_t deviceType;
				uint8_t deviceIndex;
				stream->Read(deviceType);
				stream->Read(deviceIndex);
				const DeviceId deviceId = inputManager->FindDeviceId(InputDevice::DeviceType(deviceType), deviceIndex);
				InputDevice* device = inputManager->GetDevice(deviceId);
				GAINPUT_ASSERT(device);
				device->SetSynced(true);
				devSyncedDevices.push_back(deviceId);
				GAINPUT_LOG("GAINPUT: Starting to sync device #%d.\n", deviceId);
			}
			else if (cmd == DevCmdSetDeviceButton)
			{
				uint8_t deviceType;
				uint8_t deviceIndex;
				uint32_t deviceButtonId;
				stream->Read(deviceType);
				stream->Read(deviceIndex);
				stream->Read(deviceButtonId);
				const DeviceId deviceId = inputManager->FindDeviceId(InputDevice::DeviceType(deviceType), deviceIndex);
				InputDevice* device = inputManager->GetDevice(deviceId);
				GAINPUT_ASSERT(device);
				GAINPUT_ASSERT(device->GetInputState());
				GAINPUT_ASSERT(device->GetPreviousInputState());
				if (device->GetButtonType(deviceButtonId) == BT_BOOL)
				{
					uint8_t value;
					stream->Read(value);
					bool boolValue = (0 != value);
					HandleButton(*device, *device->GetInputState(), delta, deviceButtonId, boolValue);
				}
				else
				{
					float value;
					stream->Read(value);
					HandleAxis(*device, *device->GetInputState(), delta, deviceButtonId, value);
				}
			}
		}

		bool pingFailed = false;
		if (devFrame % 100 == 0 && devConnection->IsConnected())
		{
			stream->Reset();
			stream->Write(uint8_t(DevCmdPing));
			stream->SeekBegin(0);
			if (SendMessage(*stream) == 0)
			{
				pingFailed = true;
			}
		}

		allocator->Delete(stream);

		if (readFailed || !devConnection->IsConnected() || pingFailed)
		{
			GAINPUT_LOG("GAINPUT: Disconnected\n");
			devConnection->Close();
			allocator->Delete(devConnection);
			devConnection = 0;

			for (Array<DeviceId>::iterator it = devSyncedDevices.begin();
					it != devSyncedDevices.end();
					++it)
			{
				InputDevice* device = inputManager->GetDevice(*it);
				GAINPUT_ASSERT(device);
				device->SetSynced(false);
				GAINPUT_LOG("GAINPUT: Stopped syncing device #%d.\n", *it);
			}
			devSyncedDevices.clear();

			if (devDeviceButtonListener)
			{
				inputManager->RemoveListener(devDeviceButtonListenerId);
				allocator->Delete(devDeviceButtonListener);
				devDeviceButtonListener = 0;
			}

			return;
		}
	}

	++devFrame;
}

void
DevNewMap(InputMap* inputMap)
{
	if (devMaps.find(inputMap) != devMaps.end())
	{
		return;
	}
	devMaps.push_back(inputMap);

	DevUserButtonListener* listener = allocator->New<DevUserButtonListener>(inputMap);
	devUserButtonListeners[inputMap] = listener;
	inputMap->AddListener(listener);

	if (devConnection && devSendInfos)
	{
		Stream* stream = allocator->New<MemoryStream>(1024, *allocator);
		SendMap(inputMap, stream, devConnection);
		allocator->Delete(stream);
	}
}

void
DevNewUserButton(InputMap* inputMap, UserButtonId userButton, DeviceId device, DeviceButtonId deviceButton)
{
	if (!devConnection || devMaps.find(inputMap) == devMaps.end() || !devSendInfos)
	{
		return;
	}

	Stream* stream = allocator->New<MemoryStream>(1024, *allocator);
	stream->Write(uint8_t(DevCmdUserButton));
	stream->Write(uint32_t(inputMap->GetId()));
	stream->Write(uint32_t(userButton));
	stream->Write(uint32_t(device));
	stream->Write(uint32_t(deviceButton));
	stream->Write(inputMap->GetFloat(userButton));
	stream->SeekBegin(0);
	SendMessage(*stream);
	allocator->Delete(stream);
}

void
DevRemoveUserButton(InputMap* inputMap, UserButtonId userButton)
{
	if (!devConnection || !devSendInfos)
	{
		return;
	}

	Stream* stream = allocator->New<MemoryStream>(1024, *allocator);
	stream->Write(uint8_t(DevCmdRemoveUserButton));
	stream->Write(uint32_t(inputMap->GetId()));
	stream->Write(uint32_t(userButton));
	stream->SeekBegin(0);
	SendMessage(*stream);
	allocator->Delete(stream);
}

void
DevRemoveMap(InputMap* inputMap)
{
	if (!devConnection || devMaps.find(inputMap) == devMaps.end())
	{
		return;
	}

	if (devSendInfos)
	{
		Stream* stream = allocator->New<MemoryStream>(1024, *allocator);
		stream->Write(uint8_t(DevCmdRemoveMap));
		stream->Write(uint32_t(inputMap->GetId()));
		stream->SeekBegin(0);
		SendMessage(*stream);
		allocator->Delete(stream);
	}

	devMaps.erase(devMaps.find(inputMap));

	HashMap<const InputMap*, DevUserButtonListener*>::iterator it = devUserButtonListeners.find(inputMap);
	if (it != devUserButtonListeners.end())
	{
		allocator->Delete(it->second);
		devUserButtonListeners.erase(it->first);
	}
}

void
DevNewDevice(InputDevice* device)
{
	if (devDevices.find(device) != devDevices.end())
	{
		return;
	}
	devDevices.push_back(device);

	if (devConnection && devSendInfos)
	{
		Stream* stream = allocator->New<MemoryStream>(1024, *allocator);
		SendDevice(device, stream, devConnection);
		allocator->Delete(stream);
	}
}

void
DevConnect(InputManager* manager, const char* ip, unsigned port)
{
	if (devConnection)
	{
		devConnection->Close();
		allocator->Delete(devConnection);
	}

	if (devListener)
	{
		devListener->Stop();
		allocator->Delete(devListener);
		devListener = 0;
	}

	inputManager = manager;
	allocator = &manager->GetAllocator();

	devConnection = allocator->New<NetConnection>(NetAddress(ip, port));

	if (!devConnection->Connect(false) || !devConnection->IsConnected())
	{
		devConnection->Close();
		allocator->Delete(devConnection);
		devConnection = 0;
		GAINPUT_LOG("GAINPUT: Unable to connect to %s:%d.\n", ip, port);
	}

	GAINPUT_LOG("GAINPUT: Connected to %s:%d.\n", ip, port);

	if (!devDeviceButtonListener)
	{
		devDeviceButtonListener = allocator->New<DevDeviceButtonListener>(manager);
		devDeviceButtonListenerId = inputManager->AddListener(devDeviceButtonListener);
	}
}

void
DevStartDeviceSync(DeviceId deviceId)
{
	if (devSyncedDevices.find(deviceId) != devSyncedDevices.end()
        || !devConnection)
		return;
	devSyncedDevices.push_back(deviceId);

	const InputDevice* device = inputManager->GetDevice(deviceId);
	GAINPUT_ASSERT(device);

	Stream* stream = allocator->New<MemoryStream>(32, *allocator);
	stream->Write(uint8_t(DevCmdStartDeviceSync));
	stream->Write(uint8_t(device->GetType()));
	stream->Write(uint8_t(device->GetIndex()));
	stream->SeekBegin(0);
	SendMessage(*stream);

	// Send device state
	for (DeviceButtonId buttonId = 0; buttonId < device->GetInputState()->GetButtonCount(); ++buttonId)
	{
		if (device->IsValidButtonId(buttonId))
		{
			stream->Reset();
			stream->Write(uint8_t(DevCmdSetDeviceButton));
			stream->Write(uint8_t(device->GetType()));
			stream->Write(uint8_t(device->GetIndex()));
			stream->Write(uint32_t(buttonId));
			if (device->GetButtonType(buttonId) == BT_BOOL)
			{
				stream->Write(uint8_t(device->GetBool(buttonId)));
			}
			else
			{
				stream->Write(device->GetFloat(buttonId));
			}
			stream->SeekBegin(0);
			SendMessage(*stream);
		}
	}

	allocator->Delete(stream);
}

}

#else

namespace gainput
{
void
DevSetHttp(bool /*enable*/)
{ }
}

#endif

