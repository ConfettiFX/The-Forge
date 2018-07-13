
#include <gainput/gainput.h>

#include "../samplefw/SampleFramework.h"


enum Button
{
	ButtonConfirm,
	ButtonConfirmDouble,
	ButtonConfirmExtra,
	ButtonHoldGesture,
	ButtonTapGesture,
	ButtonPinching,
	ButtonPinchScale,
	ButtonRotating,
	ButtonRotateAngle,
};


const unsigned TouchPointCount = 8;
const unsigned TouchDataElems = 4;

class MultiTouchEmulator : public gainput::InputDevice
{
public:
	MultiTouchEmulator(gainput::InputManager& manager, gainput::DeviceId device, unsigned index, gainput::InputDevice::DeviceVariant variant) : 
		gainput::InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_CUSTOM) : 0)
	{
		state_ = manager.GetAllocator().New<gainput::InputState>(manager.GetAllocator(), TouchPointCount*TouchDataElems);
		GAINPUT_ASSERT(state_);
		previousState_ = manager.GetAllocator().New<gainput::InputState>(manager.GetAllocator(), TouchPointCount*TouchDataElems);
		GAINPUT_ASSERT(previousState_);
	}

	~MultiTouchEmulator()
	{
		manager_.GetAllocator().Delete(state_);
		manager_.GetAllocator().Delete(previousState_);
	}

	void Initialize(gainput::DeviceId downDevice, gainput::DeviceButtonId downButton,
		gainput::DeviceId xAxisDevice, gainput::DeviceButtonId xAxisButton,
		gainput::DeviceId yAxisDevice, gainput::DeviceButtonId yAxisButton,
		gainput::DeviceId downDevice2, gainput::DeviceButtonId downButton2,
		gainput::DeviceId xAxisDevice2, gainput::DeviceButtonId xAxisButton2,
		gainput::DeviceId yAxisDevice2, gainput::DeviceButtonId yAxisButton2)
	{
		isDown_ = false;

		downDevice_ = downDevice;
		downButton_ = downButton;
		xAxisDevice_ = xAxisDevice;
		xAxisButton_ = xAxisButton;
		yAxisDevice_ = yAxisDevice;
		yAxisButton_ = yAxisButton;
		downDevice2_ = downDevice2;
		downButton2_ = downButton2;
		xAxisDevice2_ = xAxisDevice2;
		xAxisButton2_ = xAxisButton2;
		yAxisDevice2_ = yAxisDevice2;
		yAxisButton2_ = yAxisButton2;
	}

	DeviceType GetType() const { return DT_CUSTOM; }
	const char* GetTypeName() const { return "custom"; }

	bool IsValidButtonId(gainput::DeviceButtonId deviceButton) const
	{
		return deviceButton == gainput::Touch0Down
			|| deviceButton == gainput::Touch0X
			|| deviceButton == gainput::Touch0Y
			|| deviceButton == gainput::Touch1Down
			|| deviceButton == gainput::Touch1X
			|| deviceButton == gainput::Touch1Y;
	}

	gainput::ButtonType GetButtonType(gainput::DeviceButtonId deviceButton) const
	{
		GAINPUT_ASSERT(IsValidButtonId(deviceButton));
		return (deviceButton == gainput::Touch0Down || deviceButton == gainput::Touch1Down) ? gainput::BT_BOOL : gainput::BT_FLOAT;
	}

protected:
	void InternalUpdate(gainput::InputDeltaState* delta)
	{
		const gainput::InputDevice* downDevice = manager_.GetDevice(downDevice_);
		GAINPUT_ASSERT(downDevice);
		if (!downDevice->GetBool(downButton_) && downDevice->GetBoolPrevious(downButton_))
		{
			isDown_ = !isDown_;
			const gainput::InputDevice* xDevice = manager_.GetDevice(xAxisDevice_);
			GAINPUT_ASSERT(xDevice);
			x_ = xDevice->GetFloat(xAxisButton_);
			const gainput::InputDevice* yDevice = manager_.GetDevice(yAxisDevice_);
			GAINPUT_ASSERT(yDevice);
			y_ = yDevice->GetFloat(yAxisButton_);
		}

		state_->Set(gainput::Touch1Down, isDown_);
		state_->Set(gainput::Touch1X, x_);
		state_->Set(gainput::Touch1Y, y_);

		const gainput::InputDevice* downDevice2 = manager_.GetDevice(downDevice2_);
		GAINPUT_ASSERT(downDevice2);
		const gainput::InputDevice* xDevice2 = manager_.GetDevice(xAxisDevice2_);
		GAINPUT_ASSERT(xDevice2);
		const gainput::InputDevice* yDevice2 = manager_.GetDevice(yAxisDevice2_);
		GAINPUT_ASSERT(yDevice2);
		state_->Set(gainput::Touch0Down, downDevice2->GetBool(downButton2_));
		state_->Set(gainput::Touch0X, xDevice2->GetFloat(xAxisButton2_));
		state_->Set(gainput::Touch0Y, yDevice2->GetFloat(yAxisButton2_));
	}

	DeviceState InternalGetState() const { return DS_OK; }

private:
	bool isDown_;
	float x_;
	float y_;
	gainput::DeviceId downDevice_;
	gainput::DeviceButtonId downButton_;
	gainput::DeviceId xAxisDevice_;
	gainput::DeviceButtonId xAxisButton_;
	gainput::DeviceId yAxisDevice_;
	gainput::DeviceButtonId yAxisButton_;
	gainput::DeviceId downDevice2_;
	gainput::DeviceButtonId downButton2_;
	gainput::DeviceId xAxisDevice2_;
	gainput::DeviceButtonId xAxisButton2_;
	gainput::DeviceId yAxisDevice2_;
	gainput::DeviceButtonId yAxisButton2_;
};


void SampleMain()
{
	SfwOpenWindow("Gainput: Gesture sample");

	gainput::TrackingAllocator allocator(gainput::GetDefaultAllocator());

	gainput::InputManager manager(true, allocator);

	const gainput::DeviceId keyboardId = manager.CreateDevice<gainput::InputDeviceKeyboard>();
	const gainput::DeviceId mouseId = manager.CreateDevice<gainput::InputDeviceMouse>();

	gainput::InputDeviceTouch* touchDevice = manager.CreateAndGetDevice<gainput::InputDeviceTouch>();
	GAINPUT_ASSERT(touchDevice);
	gainput::DeviceId touchId = touchDevice->GetDeviceId();

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_WIN)
	manager.SetDisplaySize(SfwGetWidth(), SfwGetHeight());
#endif

	SfwSetInputManager(&manager);

	gainput::InputMap map(manager, "testmap", allocator);

	map.MapBool(ButtonConfirm, mouseId, gainput::MouseButtonLeft);

	gainput::DoubleClickGesture* dcg = manager.CreateAndGetDevice<gainput::DoubleClickGesture>();
	GAINPUT_ASSERT(dcg);
	dcg->Initialize(mouseId, gainput::MouseButtonLeft,
			mouseId, gainput::MouseAxisX, 0.01f,
			mouseId, gainput::MouseAxisY, 0.01f,
			500);
	map.MapBool(ButtonConfirmDouble, dcg->GetDeviceId(), gainput::DoubleClickTriggered);

	gainput::SimultaneouslyDownGesture* sdg = manager.CreateAndGetDevice<gainput::SimultaneouslyDownGesture>();
	GAINPUT_ASSERT(sdg);
	sdg->AddButton(mouseId, gainput::MouseButtonLeft);
	sdg->AddButton(keyboardId, gainput::KeyShiftL);
	map.MapBool(ButtonConfirmExtra, sdg->GetDeviceId(), gainput::SimultaneouslyDownTriggered);

	MultiTouchEmulator* mte = manager.CreateAndGetDevice<MultiTouchEmulator>();
	mte->Initialize(sdg->GetDeviceId(), gainput::SimultaneouslyDownTriggered,
			mouseId, gainput::MouseAxisX,
			mouseId, gainput::MouseAxisY,
			mouseId, gainput::MouseButtonLeft,
			mouseId, gainput::MouseAxisX,
			mouseId, gainput::MouseAxisY);

	if (!touchDevice->IsAvailable() || touchDevice->GetVariant() == gainput::InputDevice::DV_NULL)
	{
		touchId = mte->GetDeviceId();
	}

	gainput::HoldGesture* hg = manager.CreateAndGetDevice<gainput::HoldGesture>();
	GAINPUT_ASSERT(hg);
	hg->Initialize(touchId, gainput::Touch0Down,
			touchId, gainput::Touch0X, 0.1f,
			touchId, gainput::Touch0Y, 0.1f,
			true,
			800);
	map.MapBool(ButtonHoldGesture, hg->GetDeviceId(), gainput::HoldTriggered);

	gainput::TapGesture* tg = manager.CreateAndGetDevice<gainput::TapGesture>();
	GAINPUT_ASSERT(tg);
	tg->Initialize(touchId, gainput::Touch0Down,
			500);
	map.MapBool(ButtonTapGesture, tg->GetDeviceId(), gainput::TapTriggered);
	
	gainput::PinchGesture* pg = manager.CreateAndGetDevice<gainput::PinchGesture>();
	GAINPUT_ASSERT(pg);
	pg->Initialize(touchId, gainput::Touch0Down,
			touchId, gainput::Touch0X,
			touchId, gainput::Touch0Y,
			touchId, gainput::Touch1Down,
			touchId, gainput::Touch1X,
			touchId, gainput::Touch1Y);
	map.MapBool(ButtonPinching, pg->GetDeviceId(), gainput::PinchTriggered);
	map.MapFloat(ButtonPinchScale, pg->GetDeviceId(), gainput::PinchScale);

	gainput::RotateGesture* rg = manager.CreateAndGetDevice<gainput::RotateGesture>();
	GAINPUT_ASSERT(rg);
	rg->Initialize(touchId, gainput::Touch0Down,
			touchId, gainput::Touch0X,
			touchId, gainput::Touch0Y,
			touchId, gainput::Touch1Down,
			touchId, gainput::Touch1X,
			touchId, gainput::Touch1Y);
	map.MapBool(ButtonRotating, rg->GetDeviceId(), gainput::RotateTriggered);
	map.MapFloat(ButtonRotateAngle, rg->GetDeviceId(), gainput::RotateAngle);

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

		if (map.GetBoolWasDown(ButtonConfirm))
		{
			SFW_LOG("Confirmed!\n");
			SFW_LOG("Memory: %u allocs, %u deallocs, %u used bytes\n", static_cast<unsigned>(allocator.GetAllocateCount()), static_cast<unsigned>(allocator.GetDeallocateCount()), static_cast<unsigned>(allocator.GetAllocatedMemory()));
		}

		if (map.GetBoolWasDown(ButtonConfirmDouble))
		{
			SFW_LOG("Confirmed doubly!\n");
		}

		if (map.GetBoolWasDown(ButtonConfirmExtra))
		{
			SFW_LOG("Confirmed alternatively!\n");
		}

		if (map.GetBool(ButtonHoldGesture))
		{
			SFW_LOG("Hold triggered!\n");
		}

		if (map.GetBoolWasDown(ButtonTapGesture))
		{
			SFW_LOG("Tapped!\n");
		}

		if (map.GetBool(ButtonPinching))
		{
			SFW_LOG("Pinching: %f\n", map.GetFloat(ButtonPinchScale));
		}

		if (map.GetBool(ButtonRotating))
		{
			SFW_LOG("Rotation angle: %f\n", map.GetFloat(ButtonRotateAngle));
		}
	}

	SfwCloseWindow();
}


