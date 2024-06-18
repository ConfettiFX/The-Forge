

#include "../../include/gainput/gainput.h"
#include "../../include/gainput/GainputInputDeltaState.h"

#if defined(GAINPUT_PLATFORM_LINUX)
#include <time.h>
#include <X11/Xlib.h>
#include "linux/GainputInputDeviceKeyboardLinux.h"
#include "linux/GainputInputDeviceMouseLinux.h"
#include "linux/GainputInputDeviceMouseLinuxRaw.h"
#elif defined(GAINPUT_PLATFORM_WIN)
#include "windows/GainputInputDeviceKeyboardWin.h"
#include "windows/GainputInputDeviceKeyboardWinRaw.h"
#include "windows/GainputInputDeviceMouseWin.h"
#include "windows/GainputInputDeviceMouseWinRaw.h"
#include "windows/GainputInputDevicePadWin.h"
#include "windows/GainputInputDevicePadWinMsgHandler.h"
#elif defined(GAINPUT_PLATFORM_ANDROID)
#include <time.h>
#include <jni.h>
#include "android/GainputInputDeviceKeyboardAndroid.h"
#include "android/GainputInputDevicePadAndroid.h"
#include "android/GainputInputDeviceTouchAndroid.h"
static gainput::InputManager* gGainputInputManager;
#elif defined(GAINPUT_PLATFORM_QUEST)
#include <time.h>
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
#include <mach/mach.h>
#include <mach/clock.h>
#elif defined(GAINPUT_PLATFORM_GGP) || defined(GAINPUT_PLATFORM_NX64)
#include <time.h>
#elif defined(GAINPUT_PLATFORM_ORBIS)
#include <time.h>
#elif defined(GAINPUT_PLATFORM_PROSPERO)
#include <time.h>
#endif

#include <stdlib.h>
#include <math.h>

#include "dev/GainputDev.h"
#if defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_LINUX)
#include "hid/GainputHID.h"
#endif
#include "../../include/gainput/GainputHelpers.h"


namespace gainput
{
#if defined(GAINPUT_PLATFORM_WIN)
static WinMsgHandler dInputHandler;
#endif

static float checkConnectionElapsedTime = 0.0f;
static float checkConnectionTimeOut = 0.2f;

InputManager::InputManager(Allocator& allocator) :
    mAllocator(allocator),
	mDevices(mAllocator),
	mNextDeviceID(0),
	mDevListenerMetadata(NULL),
	mDevListener(NULL),
	mPadCnt(0),
	mPads(mAllocator, 4),
	mToRemove(mAllocator),
	mListeners(mAllocator),
    mNextListenerID(0),
    mSortedListeners(mAllocator),
    mModifiers(mAllocator),
    mNextModifierID(0),
    pDeltaState(NULL),
	mDeltaTimeRemainderMs(0),
    mCurrentTime(0),
    GAINPUT_CONC_CONSTRUCT(mConcurrentInputs),
    mDisplayWidth(-1),
    mDisplayHeight(-1),
    mDebugRendererEnabled(false),
    pDebugRenderer(NULL),
    pWindowInstance(NULL),
    mInitialized(false),
	mHIDDiscoveryEnabled(true)
{
	GAINPUT_DEV_INIT(this);
#ifdef GAINPUT_PLATFORM_ANDROID
	gGainputInputManager = this;
#endif
}

void InputManager::Init(void* windowInstance)
{
    GAINPUT_ASSERT(!mInitialized);

    pWindowInstance = windowInstance;
    pDeltaState = mAllocator.New<InputDeltaState>(mAllocator);
#if defined(GAINPUT_PLATFORM_MAC)
	// disable hid discovery on newer OS as it's all taken care of by GCKit
	if(IOS14_RUNTIME)
	{
		mHIDDiscoveryEnabled = false;
	}
#endif
	
#if defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_LINUX)
    HIDInit(windowInstance, this);
#endif

    mInitialized = true;
}

void InputManager::Exit()
{
    GAINPUT_ASSERT(mInitialized);

#if defined(GAINPUT_PLATFORM_WIN)
	dInputHandler.Exit();
#endif

#if defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_LINUX)
    HIDExit();
#endif
    mAllocator.Delete(pDeltaState);

	ApplyPendingDeletes();

	// clear pads that haven't been handed to the device manager
	for (int i = 0; i < mPadCnt; ++i)
		if (mPads[i]->GetDeviceId() == InvalidDeviceId)
			mDevices.insert(mPads[i]->GetDeviceId(), mPads[i]);

    for (DeviceMap::iterator it = mDevices.begin(); it != mDevices.end(); ++it)
    {
		NotifyDeviceListener(it->first, it->second, false);
        mAllocator.Delete(it->second);
    }
    mDevices.clear();

    GAINPUT_DEV_SHUTDOWN(this);
}

void InputManager::ClearAllStates(gainput::DeviceId deviceId)
{
	if (deviceId == gainput::InvalidDeviceId)
		return;

	DeviceButtonSpec buttonsDown[256];

	size_t activeButtons = GetAnyButtonDown(buttonsDown, 256);
	InputDeltaState* ds = mListeners.empty() ? 0 : pDeltaState;


	for (size_t i = 0; i < activeButtons; i++)
	{
		if (buttonsDown[i].deviceId != deviceId)
			continue;

		InputDevice * inDevice = GetDevice(buttonsDown[i].deviceId);
		if (!inDevice)
			continue;
		//get next input state
		InputState * nextDeviceState = inDevice->GetNextInputState();
		InputState * currentDeviceState = inDevice->GetInputState();
		if (!nextDeviceState || !nextDeviceState)
		{
			continue;
		}
		
		if (inDevice->GetButtonType(buttonsDown[i].buttonId) == BT_BOOL)
		{
			if(nextDeviceState)
				HandleButton(*inDevice, *nextDeviceState, ds, buttonsDown[i].buttonId, false);

			if (currentDeviceState)
				HandleButton(*inDevice, *currentDeviceState, NULL, buttonsDown[i].buttonId, false);
		}
		else
		{
			if (nextDeviceState)
				HandleAxis(*inDevice, *nextDeviceState, ds, buttonsDown[i].buttonId, 0.0f);

			if (currentDeviceState)
				HandleAxis(*inDevice, *currentDeviceState, NULL, buttonsDown[i].buttonId, 0.0f);
		}
	}

}


void InputManager::Update(float deltaTime)
{
    GAINPUT_ASSERT(mInitialized);

	// Nudge controllers to update their connection status as relevant
	// The-Forge's modification add elapsed time before checking for new/removed devices
	checkConnectionElapsedTime += deltaTime;
	if (checkConnectionElapsedTime > checkConnectionTimeOut)
	{
		for (int i = 0; i < mPadCnt; ++i)
		{
			mPads[i]->CheckConnection();
		}
		checkConnectionElapsedTime = 0.0f;
	}

	// Update mCurrentTime without running slower due to rounding errors
	{
		float integerTimeMs;
		mDeltaTimeRemainderMs = modff(mDeltaTimeRemainderMs + deltaTime * 1000.0f, &integerTimeMs);
		mCurrentTime += (uint64_t)integerTimeMs;
	}

#if defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_LINUX)
    HIDPromptForDeviceStateReports(pDeltaState);
#endif

	Change change{};
    while (GAINPUT_CONC_DEQUEUE(mConcurrentInputs, change))
    {
        if (change.type == BT_BOOL)
        {
            HandleButton(*change.device, *change.state, change.delta, change.buttonId, change.b);
        }
        else if (change.type == BT_FLOAT)
        {
            HandleAxis(*change.device, *change.state, change.delta, change.buttonId, change.f);
        }
    }
    
	InputDeltaState* ds = mListeners.empty() ? 0 : pDeltaState;

	for (DeviceMap::iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
		if (mToRemove.find(it->first) != mToRemove.end())
			continue;

		if (!it->second->IsLateUpdate())
		{
			it->second->Update(ds);
		}
	}

	GAINPUT_DEV_UPDATE(ds);

	for (HashMap<ModifierId, DeviceStateModifier*>::iterator it = mModifiers.begin();
			it != mModifiers.end();
			++it)
	{
		it->second->Update(ds);
	}

	for (DeviceMap::iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
		if (mToRemove.find(it->first) != mToRemove.end())
			continue;

		if (it->second->IsLateUpdate())
		{
			it->second->Update(ds);
		}
	}

	if (ds)
	{
		ds->NotifyListeners(deltaTime, mSortedListeners);
		ds->Clear();
	}
	

#ifdef GAINPUT_PLATFORM_IOS
	//clear buttons
	//only Does something for KeyboardIOS
	for (DeviceMap::iterator it = mDevices.begin();
		 it != mDevices.end();
		 ++it)
	{
		if (it->second->GetType() == InputDevice::DT_KEYBOARD)
		{
			it->second->ClearButtons();
		}
	}
#endif

	ApplyPendingDeletes();
}

uint64_t InputManager::GetTime() const
{
	return mCurrentTime;
}

void
InputManager::SetDeviceListener(void* metadata, DeviceListener listener)
{
    mDevListenerMetadata = metadata;
    mDevListener = listener;
}

void
InputManager::NotifyDeviceListener(DeviceId deviceId, InputDevice* device, bool doAdd)
{
    if (mDevListener)
    {
        mDevListener(mDevListenerMetadata, deviceId, device, doAdd);
    }
}

void
InputManager::CreateControllers(int cnt)
{
	mPadCnt = cnt;
	mPads.reserve(mPadCnt);

	for (int i = 0; i < mPadCnt; ++i)
	{
		gainput::InputDevicePad* newPad = tf_new(gainput::InputDevicePad, *this, InvalidDeviceId, i, InputDevice::DV_STANDARD);
		GAINPUT_ASSERT(newPad);
		mPads.push_back(newPad);
	}

#if defined(GAINPUT_PLATFORM_WIN)
	dInputHandler.Init(this, cnt);
#endif
}

DeviceId
InputManager::FindDeviceId(const char* typeName, unsigned index) const
{
	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
		if (strcmp(typeName, it->second->GetTypeName()) == 0
			&& it->second->GetIndex() == index)
		{
			return it->first;
		}
	}
	return InvalidDeviceId;
}

DeviceId
InputManager::FindDeviceId(InputDevice::DeviceType type, unsigned index) const
{
	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
		if (it->second->GetType() == type
			&& it->second->GetIndex() == index)
		{
			return it->first;
		}
	}
	return InvalidDeviceId;
}

ListenerId
InputManager::AddListener(InputListener* listener)
{
	mListeners[mNextListenerID] = listener;
	ReorderListeners();
	return mNextListenerID++;
}

void
InputManager::RemoveListener(ListenerId listenerId)
{
	mListeners.erase(listenerId);
	ReorderListeners();
}

namespace {
static int CompareListeners(const void* a, const void* b)
{
	const InputListener* listener1 = *reinterpret_cast<const InputListener* const*>(a);
	const InputListener* listener2 = *reinterpret_cast<const InputListener* const*>(b);
	return listener2->GetPriority() - listener1->GetPriority();
}
}

void
InputManager::ReorderListeners()
{
	mSortedListeners.clear();
	for (HashMap<ListenerId, InputListener*>::iterator it = mListeners.begin();
		it != mListeners.end();
		++it)
	{
		mSortedListeners.push_back(it->second);
	}

	if (mSortedListeners.empty())
	{
		return;
	}

	qsort(&mSortedListeners[0],
		mSortedListeners.size(),
		sizeof(InputListener*),
		&CompareListeners);
}

ModifierId
InputManager::AddDeviceStateModifier(DeviceStateModifier* modifier)
{
	mModifiers[mNextModifierID] = modifier;
	return mNextModifierID++;
}

void
InputManager::RemoveDeviceStateModifier(ModifierId modifierId)
{
	mModifiers.erase(modifierId);
}

size_t
InputManager::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	size_t buttonsFound = 0;
	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end() && maxButtonCount > buttonsFound;
			++it)
	{
		buttonsFound += it->second->GetAnyButtonDown(outButtons+buttonsFound, maxButtonCount-buttonsFound);
	}
	return buttonsFound;
}

unsigned
InputManager::GetDeviceCountByType(InputDevice::DeviceType type) const
{
	unsigned count = 0;
	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
		if (it->second->GetType() == type)
		{
			++count;
		}
	}
	return count;
}

#if defined(GAINPUT_PLATFORM_LINUX)
void
InputManager::HandleEvent(XEvent& event)
{
	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
#if defined(GAINPUT_DEV)
		if (it->second->IsSynced())
		{
			continue;
		}
#endif
		if (it->second->GetType() == InputDevice::DT_KEYBOARD
			&& it->second->GetVariant() == InputDevice::DV_STANDARD)
		{
			InputDeviceKeyboard* keyboard = static_cast<InputDeviceKeyboard*>(it->second);
			InputDeviceKeyboardImplLinux* keyboardImpl = static_cast<InputDeviceKeyboardImplLinux*>(keyboard->GetPimpl());
			GAINPUT_ASSERT(keyboardImpl);
			keyboardImpl->HandleEvent(event);
		}
		else if (it->second->GetType() == InputDevice::DT_MOUSE)
		{
			if(it->second->GetVariant() == InputDevice::DV_STANDARD)
			{
				InputDeviceMouse* mouse = static_cast<InputDeviceMouse*>(it->second);
				InputDeviceMouseImplLinux* mouseImpl = static_cast<InputDeviceMouseImplLinux*>(mouse->GetPimpl());
				GAINPUT_ASSERT(mouseImpl);
				mouseImpl->HandleEvent(event);
			}
			else
			{
				InputDeviceMouse* mouse = static_cast<InputDeviceMouse*>(it->second);
				InputDeviceMouseImplLinuxRaw* mouseImpl = static_cast<InputDeviceMouseImplLinuxRaw*>(mouse->GetPimpl());
				GAINPUT_ASSERT(mouseImpl);
				mouseImpl->HandleEvent(event);
			}
		}
	}
}
#endif

#if defined(GAINPUT_PLATFORM_WIN)
void
InputManager::HandleMessage(const MSG& msg)
{
    if (HIDHandleSystemMessage(&msg))
        return;

	if (msg.message == WM_DEVICECHANGE || msg.message == WM_INPUT)
	{
		dInputHandler.HandleMessage(msg);
	}

	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
#if defined(GAINPUT_DEV)
		if (it->second->IsSynced())
		{
			continue;
		}
#endif
		if (it->second->GetType() == InputDevice::DT_KEYBOARD)
		{
			InputDeviceKeyboard* keyboard = static_cast<InputDeviceKeyboard*>(it->second);
			if (it->second->GetVariant() == InputDevice::DV_STANDARD)
			{
				InputDeviceKeyboardImplWin* keyboardImpl = static_cast<InputDeviceKeyboardImplWin*>(keyboard->GetPimpl());
				GAINPUT_ASSERT(keyboardImpl);
				keyboardImpl->HandleMessage(msg);
			}
			else if (it->second->GetVariant() == InputDevice::DV_RAW)
			{
				InputDeviceKeyboardImplWinRaw* keyboardImpl = static_cast<InputDeviceKeyboardImplWinRaw*>(keyboard->GetPimpl());
				GAINPUT_ASSERT(keyboardImpl);
				keyboardImpl->HandleMessage(msg);
			}
		}
		else if (it->second->GetType() == InputDevice::DT_MOUSE)
		{
			InputDeviceMouse* mouse = static_cast<InputDeviceMouse*>(it->second);
			if (it->second->GetVariant() == InputDevice::DV_STANDARD)
			{
				InputDeviceMouseImplWin* mouseImpl = static_cast<InputDeviceMouseImplWin*>(mouse->GetPimpl());
				GAINPUT_ASSERT(mouseImpl);
				mouseImpl->HandleMessage(msg);
			}
			else if (it->second->GetVariant() == InputDevice::DV_RAW)
			{
				InputDeviceMouseImplWinRaw* mouseImpl = static_cast<InputDeviceMouseImplWinRaw*>(mouse->GetPimpl());
				GAINPUT_ASSERT(mouseImpl);
				mouseImpl->HandleMessage(msg);
			}
		}
	}
}
#endif

#if defined(GAINPUT_PLATFORM_ANDROID)
int32_t
InputManager::HandleInput(AInputEvent* event, ANativeActivity* activity)
{
	int handled = 0;
	for (DeviceMap::const_iterator it = mDevices.begin();
			it != mDevices.end();
			++it)
	{
#if defined(GAINPUT_DEV)
		if (it->second->IsSynced())
		{
			continue;
		}
#endif
		if (it->second->GetType() == InputDevice::DT_TOUCH)
		{
			InputDeviceTouch* touch = static_cast<InputDeviceTouch*>(it->second);
			InputDeviceTouchImplAndroid* touchImpl = static_cast<InputDeviceTouchImplAndroid*>(touch->GetPimpl());
			GAINPUT_ASSERT(touchImpl);
			handled |= touchImpl->HandleInput(event);
		}
		else if (it->second->GetType() == InputDevice::DT_KEYBOARD)
		{
			InputDeviceKeyboard* keyboard = static_cast<InputDeviceKeyboard*>(it->second);
			InputDeviceKeyboardImplAndroid* keyboardImpl = static_cast<InputDeviceKeyboardImplAndroid*>(keyboard->GetPimpl());
			GAINPUT_ASSERT(keyboardImpl);
			handled |= keyboardImpl->HandleInput(event, activity);
		}
		else if (it->second->GetType() == InputDevice::DT_PAD)
		{
			InputDevicePad* pad = static_cast<InputDevicePad*>(it->second);
			InputDevicePadImplAndroid* padImpl = static_cast<InputDevicePadImplAndroid*>(pad->GetPimpl());
			GAINPUT_ASSERT(padImpl);
			handled |= padImpl->HandleInput(event);
		}
	}
	return handled;
}

void
InputManager::HandleDeviceInput(DeviceInput const& input)
{
	DeviceId devId = FindDeviceId(input.deviceType, input.deviceIndex);
	if (devId == InvalidDeviceId)
	{
		return;
	}

	InputDevice* device = GetDevice(devId);
	if (!device)
	{
		return;
	}

#if defined(GAINPUT_DEV)
	if (device->IsSynced())
	{
		return;
	}
#endif

    InputState* state = device->GetNextInputState();
    if (!state)
    {
        state = device->GetInputState();
    }
	if (!state)
	{
		return;
	}

	if (input.buttonType == BT_BOOL)
	{
		EnqueueConcurrentChange(*device, *state, pDeltaState, input.buttonId, input.value.b);
	}
	else if (input.buttonType == BT_FLOAT)
	{
		EnqueueConcurrentChange(*device, *state, pDeltaState, input.buttonId, input.value.f);
	}
	else if (input.buttonType == BT_COUNT && input.deviceType == InputDevice::DT_PAD)
	{
		InputDevicePad* pad = static_cast<InputDevicePad*>(device);
		InputDevicePadImplAndroid* impl = static_cast<InputDevicePadImplAndroid*>(pad->GetPimpl());
		GAINPUT_ASSERT(impl);
		if (input.value.b)
		{
			impl->SetState(InputDevice::DeviceState::DS_OK);
		}
		else
		{
			impl->SetState(InputDevice::DeviceState::DS_UNAVAILABLE);
		}
	}
}

#endif

void
InputManager::ConnectForStateSync(const char* ip, unsigned port)
{
	GAINPUT_UNUSED(ip); GAINPUT_UNUSED(port);
	GAINPUT_DEV_CONNECT(this, ip, port);
}

void
InputManager::StartDeviceStateSync(DeviceId deviceId)
{
	GAINPUT_ASSERT(GetDevice(deviceId));
	GAINPUT_ASSERT(GetDevice(deviceId)->GetType() != InputDevice::DT_GESTURE);
	GAINPUT_DEV_START_SYNC(deviceId);
}

void
InputManager::SetDebugRenderingEnabled(bool enabled)
{
	mDebugRendererEnabled = enabled;
	if (enabled)
	{
		GAINPUT_ASSERT(pDebugRenderer);
	}
}

void
InputManager::SetDebugRenderer(DebugRenderer* debugRenderer)
{
	pDebugRenderer = debugRenderer;
}

void
InputManager::EnqueueConcurrentChange(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, bool value)
{
    Change change;
    change.device = &device;
    change.state = &state;
    change.delta = delta;
    change.buttonId = buttonId;
    change.type = BT_BOOL;
    change.b = value;
    GAINPUT_CONC_ENQUEUE(mConcurrentInputs, change);
}

void
InputManager::EnqueueConcurrentChange(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, float value)
{
    Change change;
    change.device = &device;
    change.state = &state;
    change.delta = delta;
    change.buttonId = buttonId;
    change.type = BT_FLOAT;
    change.f = value;
    GAINPUT_CONC_ENQUEUE(mConcurrentInputs, change);
}

void
InputManager::ApplyPendingDeletes()
{
	// remove old devices
	for (uint32_t i = 0; i < mToRemove.size(); ++i)
	{
		DeviceId deviceId = mToRemove[i];
		mDevices.erase(deviceId);
	}
	mToRemove.clear();
}

}

#if defined(GAINPUT_PLATFORM_ANDROID)
extern "C" {
JNIEXPORT void JNICALL
Java_de_johanneskuhlmann_gainput_Gainput_nativeOnInputBool(JNIEnv * /*env*/, jobject /*thiz*/,
                                                           jint deviceType, jint deviceIndex,
                                                           jint buttonId, jboolean value)
{
	if (!gGainputInputManager)
	{
		return;
	}
	using namespace gainput;
    InputManager::DeviceInput input;
    input.deviceType = static_cast<InputDevice::DeviceType>(deviceType);
    input.deviceIndex = deviceIndex;
    input.buttonType = BT_BOOL;
	if (input.deviceType == InputDevice::DT_KEYBOARD)
	{
		DeviceId deviceId = gGainputInputManager->FindDeviceId(input.deviceType, deviceIndex);
		if (deviceId != InvalidDeviceId)
		{
			InputDevice* device = gGainputInputManager->GetDevice(deviceId);
			if (device)
			{
				InputDeviceKeyboard* keyboard = static_cast<InputDeviceKeyboard*>(device);
				InputDeviceKeyboardImplAndroid* keyboardImpl = static_cast<InputDeviceKeyboardImplAndroid*>(keyboard->GetPimpl());
				GAINPUT_ASSERT(keyboardImpl);
				DeviceButtonId newId = keyboardImpl->Translate(buttonId);
				if (newId != InvalidDeviceButtonId)
				{
					buttonId = newId;
				}
			}
		}
	}
    input.buttonId = buttonId;
    input.value.b = value;
    gGainputInputManager->HandleDeviceInput(input);
}

JNIEXPORT void JNICALL
Java_de_johanneskuhlmann_gainput_Gainput_nativeOnInputFloat(JNIEnv * /*env*/, jobject /*thiz*/,
                                                            jint deviceType, jint deviceIndex,
                                                            jint buttonId, jfloat value)
{
	if (!gGainputInputManager)
	{
		return;
	}
	using namespace gainput;
    InputManager::DeviceInput input;
    input.deviceType = static_cast<InputDevice::DeviceType>(deviceType);
    input.deviceIndex = deviceIndex;
    input.buttonType = BT_FLOAT;
    input.buttonId = buttonId;
    input.value.f = value;
    gGainputInputManager->HandleDeviceInput(input);
}

JNIEXPORT void JNICALL
Java_de_johanneskuhlmann_gainput_Gainput_nativeOnDeviceChanged(JNIEnv * /*env*/, jobject /*thiz*/,
                                                               jint deviceId, jboolean value)
{
	if (!gGainputInputManager)
	{
		return;
	}
    using namespace gainput;
    InputManager::DeviceInput input;
    input.deviceType = InputDevice::DT_PAD;
    input.deviceIndex = deviceId;
    input.buttonType = BT_COUNT;
    input.value.b = value;
    gGainputInputManager->HandleDeviceInput(input);
}
}
#endif
