
#ifndef GAINPUTINPUTDEVICEPADWINMSGHANDLER_H_
#define GAINPUTINPUTDEVICEPADWINMSGHANDLER_H_

#include "GainputInputDevicePadWin.h"

namespace gainput
{

class WinMsgHandler
{
	InputManager* manager = NULL;
	int dIPadMax = 0;
	int dIPadCnt = 0;
	InputDevicePad** dInputPads = NULL;
	DirectInputInitializer* instance;

public:

	void Init(InputManager* man, int maxPadCnt)
	{
		manager = man;
		dIPadMax = maxPadCnt;
		dIPadCnt = 0;
		dInputPads = (InputDevicePad**)tf_calloc(dIPadMax, sizeof(InputDevicePad*));

		for (int i = 0; i < dIPadMax; ++i)
		{
			dInputPads[i] = tf_new(gainput::InputDevicePad, *manager, -1, InputDevice::AutoIndex, InputDevice::DV_STANDARD);
			GAINPUT_ASSERT(dInputPads[i]);
		}

		instance = DirectInputInitializer::GetInstance(man->GetWindowsInstance());
		GAINPUT_ASSERT(instance);
	}

	void Exit()
	{
		if (manager)
		{
			for (int i = 0; i < dIPadMax; ++i)
			{
				tf_delete(dInputPads[i]);
				dInputPads[i] = NULL;
			}

			tf_free(dInputPads);
		}
	}

	void HandleMessage(const MSG& msg)
	{
		switch (msg.wParam)
		{
		case DBT_DEVICEARRIVAL:
		case DBT_DEVICEREMOVECOMPLETE:
			// remove
			Clear();
			// recount
			dIPadCnt = instance->DInputCount();
			if (dIPadCnt > dIPadMax)
				dIPadCnt = dIPadMax;
			// then reassign & re-add
			ReAdd();
			break;
		default:
			// NOTE: not actually very useful
			if (msg.message == WM_INPUT &&
				dIPadCnt > 0)
			{
				InputDevicePadImplWin* impl = (InputDevicePadImplWin*)dInputPads[0]->GetPimpl();
				impl->ParseMessage((void*)msg.lParam);
			}
			break;
		}
	}

	void Clear()
	{
		for (int i = 0; i < dIPadCnt; ++i)
		{
			manager->RemoveDevice(dInputPads[i]->GetDeviceId());
			dInputPads[i]->OverrideDeviceId(InvalidDeviceId);
		}

		instance->DInputClean();
	}

	void ReAdd()
	{
		for (int i = 0; i < dIPadCnt; ++i)
		{
			InputDevicePadImplWin* impl = (InputDevicePadImplWin*)dInputPads[i]->GetPimpl();
			impl->ForceDInput(i);

			DeviceId newId = manager->GetNextId();
			dInputPads[i]->OverrideDeviceId(newId);
			manager->AddDevice(newId, dInputPads[i]);
		}
	}
};

} // namespace gainput

#endif
