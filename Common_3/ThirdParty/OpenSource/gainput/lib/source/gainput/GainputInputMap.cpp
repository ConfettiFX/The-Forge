
#include "../../include/gainput/gainput.h"
#include "dev/GainputDev.h"

#include <stdlib.h>

namespace
{
	template<class T> T Min(const T&a, const T& b) { return a < b ? a : b; }
	template<class T> T Max(const T&a, const T& b) { return a < b ? b : a; }
}

namespace gainput
{

class MappedInput
{
public:
	DeviceId device;
	DeviceButtonId deviceButton;

	float rangeMin;
	float rangeMax;

	FilterFunc_T filterFunc;
	void* filterUserData;
};

typedef Array<MappedInput> MappedInputList;

class UserButton
{
public:

	UserButtonId userButton;
	MappedInputList inputs;
	InputMap::UserButtonPolicy policy;
	float deadZone;

	UserButton(Allocator& allocator) :
		inputs(allocator),
		deadZone(0.0f)
	{ }
};


InputMap::InputMap(InputManager* manager, const char* name, Allocator& allocator) :
	manager_(*manager),
	name_(0),
	allocator_(allocator),
	userButtons_(allocator_),
	nextUserButtonId_(0),
	listeners_(allocator_),
	sortedListeners_(allocator_),
	nextListenerId_(0),
	managerListener_(0)
{
	static unsigned nextId = 0;
	id_ = nextId++;

	if (name)
	{
		uint32_t nameSize = (uint32_t)strlen(name) + 1;
		name_ = static_cast<char*>(allocator_.Allocate(nameSize));
		strncpy(name_, name, nameSize);
	}
	GAINPUT_DEV_NEW_MAP(this);
}

InputMap::InputMap(InputManager& manager, const char* name, Allocator& allocator) :
	manager_(manager),
	name_(0),
	allocator_(allocator),
	userButtons_(allocator_),
	nextUserButtonId_(0),
	listeners_(allocator_),
	sortedListeners_(allocator_),
	nextListenerId_(0),
	managerListener_(0)
{
	static unsigned nextId = 0;
	id_ = nextId++;

	if (name)
	{
		uint32_t nameSize =(uint32_t)strlen(name) + 1;
		name_ = static_cast<char*>(allocator_.Allocate(nameSize));
		strncpy(name_, name, nameSize);
	}
	GAINPUT_DEV_NEW_MAP(this);
}

InputMap::~InputMap()
{
	GAINPUT_DEV_REMOVE_MAP(this);
	Clear();
	allocator_.Deallocate(name_);

	if (managerListener_)
	{
		manager_.RemoveListener(managerListenerId_);
		allocator_.Delete(managerListener_);
	}
}

void
InputMap::Clear()
{
	for (UserButtonMap::iterator it = userButtons_.begin();
			it != userButtons_.end();
			++it)
	{
		allocator_.Delete(it->second);
		GAINPUT_DEV_REMOVE_USER_BUTTON(this, it->first);
	}
	userButtons_.clear();
	nextUserButtonId_ = 0;
}

bool
InputMap::MapBool(UserButtonId userButton, DeviceId device, DeviceButtonId deviceButton)
{
	UserButton* ub = GetUserButton(userButton);

	if (!ub)
	{
		ub = allocator_.New<UserButton>(allocator_);
		GAINPUT_ASSERT(ub);
		ub->userButton = nextUserButtonId_++;
		ub->policy = UBP_FIRST_DOWN;
		userButtons_[userButton] = ub;
	}

	MappedInput mi;
	mi.device = device;
	mi.deviceButton = deviceButton;
	ub->inputs.push_back(mi);

	GAINPUT_DEV_NEW_USER_BUTTON(this, userButton, device, deviceButton);

	return true;
}

bool
InputMap::MapFloat(UserButtonId userButton, DeviceId device, DeviceButtonId deviceButton, float min, float max, FilterFunc_T filterFunc, void* filterUserData)
{
	UserButton* ub = GetUserButton(userButton);

	if (!ub)
	{
		ub = allocator_.New<UserButton>(allocator_);
		GAINPUT_ASSERT(ub);
		ub->userButton = nextUserButtonId_++;
		ub->policy = UBP_FIRST_DOWN;
		userButtons_[userButton] = ub;
	}

	MappedInput mi;
	mi.device = device;
	mi.deviceButton = deviceButton;
	mi.rangeMin = min;
	mi.rangeMax = max;
	mi.filterFunc = filterFunc;
	mi.filterUserData = filterUserData;
	ub->inputs.push_back(mi);

	GAINPUT_DEV_NEW_USER_BUTTON(this, userButton, device, deviceButton);

	return true;
}

void
InputMap::Unmap(UserButtonId userButton)
{
	UserButton* ub = GetUserButton(userButton);
	if (ub)
	{
		allocator_.Delete(ub);
		userButtons_.erase(userButton);
		GAINPUT_DEV_REMOVE_USER_BUTTON(this, userButton);
	}
}

bool
InputMap::IsMapped(UserButtonId userButton) const
{
	return GetUserButton(userButton) != 0;
}

size_t
InputMap::GetMappings(UserButtonId userButton, DeviceButtonSpec* outButtons, size_t maxButtonCount) const
{
	size_t buttonCount = 0;
	const UserButton* ub = GetUserButton(userButton);
	if (!ub)
	{
		return 0;
	}
	for (MappedInputList::const_iterator it = ub->inputs.begin();
			it != ub->inputs.end() && buttonCount < maxButtonCount;
			++it, ++buttonCount)
	{
		const MappedInput& mi = *it;
		outButtons[buttonCount].deviceId = mi.device;
		outButtons[buttonCount].buttonId = mi.deviceButton;
	}
	return buttonCount;
}

bool
InputMap::SetUserButtonPolicy(UserButtonId userButton, UserButtonPolicy policy)
{
	UserButton* ub = GetUserButton(userButton);
	if (!ub)
	{
		return false;
	}
	ub->policy = policy;
	return true;
}

bool
InputMap::SetDeadZone(UserButtonId userButton, float deadZone)
{
	UserButton* ub = GetUserButton(userButton);
	if (!ub)
	{
		return false;
	}
	ub->deadZone = deadZone;
	return true;
}

bool
InputMap::GetBool(UserButtonId userButton) const
{
	const UserButton* ub = GetUserButton(userButton);
	GAINPUT_ASSERT(ub);
	for (MappedInputList::const_iterator it = ub->inputs.begin();
			it != ub->inputs.end();
			++it)
	{
		const MappedInput& mi = *it;
		const InputDevice* device = manager_.GetDevice(mi.device);
		GAINPUT_ASSERT(device);
		if (device->GetBool(mi.deviceButton))
		{
			return true;
		}
	}
	return false;
}

bool
InputMap::GetBoolIsNew(UserButtonId userButton) const
{
	const UserButton* ub = GetUserButton(userButton);
	GAINPUT_ASSERT(ub);
	for (MappedInputList::const_iterator it = ub->inputs.begin();
			it != ub->inputs.end();
			++it)
	{
		const MappedInput& mi= *it;
		const InputDevice* device = manager_.GetDevice(mi.device);
		GAINPUT_ASSERT(device);
		if (device->GetBool(mi.deviceButton)
				&& !device->GetBoolPrevious(mi.deviceButton))
		{
			return true;
		}
	}
	return false;
}

bool
InputMap::GetBoolPrevious(UserButtonId userButton) const
{
	const UserButton* ub = GetUserButton(userButton);
	GAINPUT_ASSERT(ub);
	for (MappedInputList::const_iterator it = ub->inputs.begin();
			it != ub->inputs.end();
			++it)
	{
		const MappedInput& mi= *it;
		const InputDevice* device = manager_.GetDevice(mi.device);
		GAINPUT_ASSERT(device);
		if (device->GetBoolPrevious(mi.deviceButton))
		{
			return true;
		}
	}
	return false;
}

bool
InputMap::GetBoolWasDown(UserButtonId userButton) const
{
	const UserButton* ub = GetUserButton(userButton);
	GAINPUT_ASSERT(ub);
	for (MappedInputList::const_iterator it = ub->inputs.begin();
			it != ub->inputs.end();
			++it)
	{
		const MappedInput& mi= *it;
		const InputDevice* device = manager_.GetDevice(mi.device);
		GAINPUT_ASSERT(device);
		if (!device->GetBool(mi.deviceButton)
				&& device->GetBoolPrevious(mi.deviceButton))
		{
			return true;
		}
	}
	return false;
}

float
InputMap::GetFloat(UserButtonId userButton) const
{
	return GetFloatState(userButton, false);
}

float
InputMap::GetFloatPrevious(UserButtonId userButton) const
{
	return GetFloatState(userButton, true);
}

float
InputMap::GetFloatDelta(UserButtonId userButton) const
{
	return GetFloat(userButton) - GetFloatPrevious(userButton);
}

float
InputMap::GetFloatState(UserButtonId userButton, bool previous) const
{
	float value = 0.0f;
	int downCount = 0;
	const UserButton* ub = GetUserButton(userButton);
	GAINPUT_ASSERT(ub);
	for (MappedInputList::const_iterator it = ub->inputs.begin();
			it != ub->inputs.end();
			++it)
	{
		const MappedInput& mi= *it;
		const InputDevice* device = manager_.GetDevice(mi.device);
		GAINPUT_ASSERT(device);

		bool down = false;
		float deviceValue = 0.0f;
		if (device->GetButtonType(mi.deviceButton) == BT_BOOL)
		{
			down = previous ? device->GetBoolPrevious(mi.deviceButton) : device->GetBool(mi.deviceButton);
			deviceValue = down ? mi.rangeMax : mi.rangeMin;
		}
		else
		{
			const float tmpValue = previous ? device->GetFloatPrevious(mi.deviceButton) : device->GetFloat(mi.deviceButton);
			if (tmpValue != 0.0f)
			{
				GAINPUT_ASSERT(device->GetButtonType(mi.deviceButton) == BT_FLOAT);
				deviceValue = mi.rangeMin + tmpValue*mi.rangeMax;
				down = true;
			}
		}

		if (mi.filterFunc)
		{
			deviceValue = mi.filterFunc(deviceValue, mi.filterUserData);
		}

		if (down)
		{
			++downCount;
			if (ub->policy == UBP_FIRST_DOWN)
			{
				value = deviceValue;
				break;
			}
			else if (ub->policy == UBP_MAX)
			{
				if (Abs(deviceValue) > Abs(value))
				{
					value = deviceValue;
				}
			}
			else if (ub->policy == UBP_MIN)
			{
				if (downCount == 1)
				{
					value = deviceValue;
				}
				else
				{
					if (Abs(deviceValue) < Abs(value))
					{
						value = deviceValue;
					}
				}
			}
			else if (ub->policy == UBP_AVERAGE)
			{
				value += deviceValue;
			}
		}
	}

	if (ub->policy == UBP_AVERAGE && downCount)
	{
		value /= float(downCount);
	}

	if (Abs(value) <= ub->deadZone)
	{
		value = 0.0f;
	}

	return value;
}

size_t
InputMap::GetUserButtonName(UserButtonId userButton, char* buffer, size_t bufferLength) const
{
	const UserButton* ub = GetUserButton(userButton);
	GAINPUT_ASSERT(ub);
    MappedInputList::const_iterator it = ub->inputs.begin();
    if(it != ub->inputs.end())
    {
        const MappedInput& mi = *it;
        const InputDevice* device = manager_.GetDevice(mi.device);
        GAINPUT_ASSERT(device);
        return device->GetButtonName(mi.deviceButton, buffer, bufferLength);
    }
    
	return 0;
}

UserButtonId
InputMap::GetUserButtonId(DeviceId device, DeviceButtonId deviceButton) const
{
	for (UserButtonMap::const_iterator it = userButtons_.begin();
			it != userButtons_.end();
			++it)
	{
		const UserButton* ub = it->second;
		for (MappedInputList::const_iterator it2 = ub->inputs.begin();
				it2 != ub->inputs.end();
				++it2)
		{
			const MappedInput& mi = *it2;
			if (mi.device == device && mi.deviceButton == deviceButton)
			{
				return it->first;
			}
		}
	}
	return InvalidUserButtonId;
}

namespace {

class ManagerToMapListener : public InputListener
{
public:
	ManagerToMapListener(InputMap& inputMap, Array<MappedInputListener*>& listeners) :
		inputMap_(inputMap),
		listeners_(listeners)
	{ }

	bool OnDeviceButtonBool(DeviceId device, DeviceButtonId deviceButton, bool oldValue, bool newValue)
	{
		const UserButtonId userButton = inputMap_.GetUserButtonId(device, deviceButton);
		if (userButton == InvalidUserButtonId)
		{
			return true;
		}
		for (Array<MappedInputListener*>::iterator it = listeners_.begin();
				it != listeners_.end();
				++it)
		{
			if(!(*it)->OnUserButtonBool(userButton, oldValue, newValue))
			{
				break;
			}
		}
		return true;
	}

	bool OnDeviceButtonFloat(DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue)
	{
		const UserButtonId userButton = inputMap_.GetUserButtonId(device, deviceButton);
		if (userButton == InvalidUserButtonId)
		{
			return true;
		}
		for (Array<MappedInputListener*>::iterator it = listeners_.begin();
				it != listeners_.end();
				++it)
		{
			if (!(*it)->OnUserButtonFloat(userButton, oldValue, newValue))
			{
				break;
			}
		}
		return true;
	}

private:
	InputMap& inputMap_;
	Array<MappedInputListener*>& listeners_;
};

}

unsigned
InputMap::AddListener(MappedInputListener* listener)
{
	if (!managerListener_)
	{
		managerListener_ = allocator_.New<ManagerToMapListener>(*this, sortedListeners_);
		managerListenerId_ = manager_.AddListener(managerListener_);
	}
	listeners_[nextListenerId_] = listener;
	ReorderListeners();
	return nextListenerId_++;
}

void
InputMap::RemoveListener(unsigned listenerId)
{
	listeners_.erase(listenerId);
	ReorderListeners();

	if (listeners_.empty())
	{
		manager_.RemoveListener(managerListenerId_);
		allocator_.Delete(managerListener_);
		managerListener_ = 0;
	}
}

namespace {
static int CompareListeners(const void* a, const void* b)
{
	const MappedInputListener* listener1 = *reinterpret_cast<const MappedInputListener* const*>(a);
	const MappedInputListener* listener2 = *reinterpret_cast<const MappedInputListener* const*>(b);
	return listener2->GetPriority() - listener1->GetPriority();
}
}

void
InputMap::ReorderListeners()
{
	sortedListeners_.clear();
	for (HashMap<ListenerId, MappedInputListener*>::iterator it = listeners_.begin();
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
		sizeof(MappedInputListener*), 
		&CompareListeners);
}

UserButton*
InputMap::GetUserButton(UserButtonId userButton)
{
	UserButtonMap::iterator it = userButtons_.find(userButton);
	if (it == userButtons_.end())
	{
		return 0;
	}
	return it->second;
}

const UserButton*
InputMap::GetUserButton(UserButtonId userButton) const
{
	UserButtonMap::const_iterator it = userButtons_.find(userButton);
	if (it == userButtons_.end())
	{
		return 0;
	}
	return it->second;
}

}

