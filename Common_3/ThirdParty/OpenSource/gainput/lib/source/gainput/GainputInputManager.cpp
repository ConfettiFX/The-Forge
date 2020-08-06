

#include "../../include/gainput/gainput.h"
#include "../../include/gainput/GainputInputDeltaState.h"

#if defined(GAINPUT_PLATFORM_LINUX)
#include <time.h>
#include <X11/Xlib.h>
#include "keyboard/GainputInputDeviceKeyboardLinux.h"
#include "mouse/GainputInputDeviceMouseLinux.h"
#include "mouse/GainputInputDeviceMouseLinuxRaw.h"
#elif defined(GAINPUT_PLATFORM_WIN)
#include "keyboard/GainputInputDeviceKeyboardWin.h"
#include "keyboard/GainputInputDeviceKeyboardWinRaw.h"
#include "mouse/GainputInputDeviceMouseWin.h"
#include "mouse/GainputInputDeviceMouseWinRaw.h"
#include "pad/GainputInputDevicePadWin.h"
#elif defined(GAINPUT_PLATFORM_ANDROID)
#include <time.h>
#include <jni.h>
#include "keyboard/GainputInputDeviceKeyboardAndroid.h"
#include "pad/GainputInputDevicePadAndroid.h"
#include "touch/GainputInputDeviceTouchAndroid.h"
static gainput::InputManager* gGainputInputManager;
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

#include "dev/GainputDev.h"
#include "../../include/gainput/GainputHelpers.h"


namespace gainput
{

InputManager::InputManager(bool useSystemTime, Allocator& allocator) :
		allocator_(allocator),
		devices_(allocator_),
		nextDeviceId_(0),
		listeners_(allocator_),
		nextListenerId_(0),
		sortedListeners_(allocator_),
		modifiers_(allocator_),
		nextModifierId_(0),
		deltaState_(allocator_.New<InputDeltaState>(allocator_)),
		currentTime_(0),
        GAINPUT_CONC_CONSTRUCT(concurrentInputs_),
		displayWidth_(-1),
		displayHeight_(-1),
		useSystemTime_(useSystemTime),
		debugRenderingEnabled_(false),
		debugRenderer_(0)
{
	GAINPUT_DEV_INIT(this);
#ifdef GAINPUT_PLATFORM_ANDROID
	gGainputInputManager = this;
#endif
}

InputManager::~InputManager()
{
	allocator_.Delete(deltaState_);

	for (DeviceMap::iterator it = devices_.begin();
			it != devices_.end();
			++it)
	{
		allocator_.Delete(it->second);
	}

	GAINPUT_DEV_SHUTDOWN(this);
}

void InputManager::ClearAllStates(gainput::DeviceId deviceId)
{
	if (deviceId == gainput::InvalidDeviceId)
		return;

	DeviceButtonSpec buttonsDown[256];

	size_t activeButtons = GetAnyButtonDown(buttonsDown, 256);
	InputDeltaState* ds = listeners_.empty() ? 0 : deltaState_;


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


void
InputManager::Update()
{
    Change change;
    while (GAINPUT_CONC_DEQUEUE(concurrentInputs_, change))
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
    
	InputDeltaState* ds = listeners_.empty() ? 0 : deltaState_;

	for (DeviceMap::iterator it = devices_.begin();
			it != devices_.end();
			++it)
	{
		if (!it->second->IsLateUpdate())
		{
			it->second->Update(ds);
		}
	}

	GAINPUT_DEV_UPDATE(ds);

	for (HashMap<ModifierId, DeviceStateModifier*>::iterator it = modifiers_.begin();
			it != modifiers_.end();
			++it)
	{
		it->second->Update(ds);
	}

	for (DeviceMap::iterator it = devices_.begin();
			it != devices_.end();
			++it)
	{
		if (it->second->IsLateUpdate())
		{
			it->second->Update(ds);
		}
	}

	if (ds)
	{
		ds->NotifyListeners(sortedListeners_);
		ds->Clear();
	}
	

#ifdef GAINPUT_PLATFORM_IOS
	//clear buttons
	//only Does something for KeyboardIOS
	for (DeviceMap::iterator it = devices_.begin();
		 it != devices_.end();
		 ++it)
	{
		if (it->second->GetType() == InputDevice::DT_KEYBOARD)
		{
			it->second->ClearButtons();
		}
	}
#endif
}

void
InputManager::Update(uint64_t deltaTime)
{
	GAINPUT_ASSERT(useSystemTime_ == false);
	currentTime_ += deltaTime;
	Update();
}

uint64_t
InputManager::GetTime() const
{
	if (useSystemTime_)
	{
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_GGP) || defined(GAINPUT_PLATFORM_ORBIS) || defined(GAINPUT_PLATFORM_PROSPERO)
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
	{
		return -1;
	}

	uint64_t t = ts.tv_sec*1000ul + ts.tv_nsec/1000000ul;
	return t;
#elif defined(GAINPUT_PLATFORM_WIN) || defined (GAINPUT_PLATFORM_XBOX_ONE)
	static LARGE_INTEGER perfFreq = { 0 };
	if (perfFreq.QuadPart == 0)
	{
		QueryPerformanceFrequency(&perfFreq);
		GAINPUT_ASSERT(perfFreq.QuadPart != 0);
	}
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	double t = 1000.0 * double(count.QuadPart) / double(perfFreq.QuadPart);
	return static_cast<uint64_t>(t);
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	clock_serv_t cclock;
	mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	uint64_t t = mts.tv_sec*1000ul + mts.tv_nsec/1000000ul;
	return t;
#elif defined(GAINPUT_PLATFORM_NX64)	
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
	{
		return -1;
	}

	uint64_t t = ts.tv_sec * 1000ul + ts.tv_nsec / 1000000ul;
	return t;
#else
#error Gainput: No time support
#endif
}
	else
	{
		return currentTime_;
	}
}

DeviceId
InputManager::FindDeviceId(const char* typeName, unsigned index) const
{
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end();
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
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end();
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
	listeners_[nextListenerId_] = listener;
	ReorderListeners();
	return nextListenerId_++;
}

void
InputManager::RemoveListener(ListenerId listenerId)
{
	listeners_.erase(listenerId);
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
	sortedListeners_.clear();
	for (HashMap<ListenerId, InputListener*>::iterator it = listeners_.begin();
		it != listeners_.end();
		++it)
	{
		sortedListeners_.push_back(it->second);
	}

	if (sortedListeners_.empty())
	{
		return;
	}

	qsort(&sortedListeners_[0], 
		sortedListeners_.size(), 
		sizeof(InputListener*), 
		&CompareListeners);
}

ModifierId
InputManager::AddDeviceStateModifier(DeviceStateModifier* modifier)
{
	modifiers_[nextModifierId_] = modifier;
	return nextModifierId_++;
}

void
InputManager::RemoveDeviceStateModifier(ModifierId modifierId)
{
	modifiers_.erase(modifierId);
}

size_t
InputManager::GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	size_t buttonsFound = 0;
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end() && maxButtonCount > buttonsFound;
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
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end();
			++it)
	{
		if (it->second->GetType() == type)
		{
			++count;
		}
	}
	return count;
}

void
InputManager::DeviceCreated(InputDevice* device)
{
	GAINPUT_UNUSED(device);
	GAINPUT_DEV_NEW_DEVICE(device);
}

#if defined(GAINPUT_PLATFORM_LINUX)
void
InputManager::HandleEvent(XEvent& event)
{
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end();
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
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end();
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
		else if (it->second->GetType() == InputDevice::DT_PAD)
		{
			if (msg.message == WM_DEVICECHANGE || msg.message == WM_INPUT)
			{
				InputDevicePad* pad = static_cast<InputDevicePad*>(it->second);
				InputDevicePadImplWin* padImpl = static_cast<InputDevicePadImplWin*>(pad->GetPimpl());
				GAINPUT_ASSERT(padImpl);
				padImpl->HandleMessage(msg);
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
	for (DeviceMap::const_iterator it = devices_.begin();
			it != devices_.end();
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
		EnqueueConcurrentChange(*device, *state, deltaState_, input.buttonId, input.value.b);
	}
	else if (input.buttonType == BT_FLOAT)
	{
		EnqueueConcurrentChange(*device, *state, deltaState_, input.buttonId, input.value.f);
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
	debugRenderingEnabled_ = enabled;
	if (enabled)
	{
		GAINPUT_ASSERT(debugRenderer_);
	}
}

void
InputManager::SetDebugRenderer(DebugRenderer* debugRenderer)
{
	debugRenderer_ = debugRenderer;
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
    GAINPUT_CONC_ENQUEUE(concurrentInputs_, change);
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
    GAINPUT_CONC_ENQUEUE(concurrentInputs_, change);
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
