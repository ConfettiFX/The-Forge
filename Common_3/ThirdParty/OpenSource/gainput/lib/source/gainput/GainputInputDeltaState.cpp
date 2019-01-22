
#include "../../include/gainput/gainput.h"
#include "../../include/gainput/GainputInputDeltaState.h"


namespace gainput
{

InputDeltaState::InputDeltaState(Allocator& allocator) :
	changes_(allocator),
	gestureChanges_(allocator)
{
}

void
InputDeltaState::AddChange(DeviceId device, DeviceButtonId deviceButton, bool oldValue, bool newValue)
{
	Change change;
	change.device = device;
	change.deviceButton = deviceButton;
	change.type = BT_BOOL;
	change.oldValue.b = oldValue;
	change.newValue.b = newValue;
	changes_.push_back(change);
}

void
InputDeltaState::AddChange(DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue)
{
	Change change;
	change.device = device;
	change.deviceButton = deviceButton;
	change.type = BT_FLOAT;
	change.oldValue.f = oldValue;
	change.newValue.f = newValue;
	changes_.push_back(change);
}
	
void
InputDeltaState::AddChange(DeviceId device, DeviceButtonId deviceButton, const GestureChange& value)
{
	Change change;
	change.device = device;
	change.deviceButton = deviceButton;
	change.type = BT_GESTURE;
	change.g = value;
	gestureChanges_.push_back(change);
}

void
InputDeltaState::Clear()
{
	changes_.clear();
	gestureChanges_.clear();
}

void
InputDeltaState::NotifyListeners(Array<InputListener*>& listeners) const
{
	// Always broadcast gestures first
	for (Array<Change>::const_iterator it = gestureChanges_.begin();
		 it != gestureChanges_.end();
		 ++it)
	{
		const Change& change = *it;
		for (Array<InputListener*>::iterator it2 = listeners.begin();
			 it2 != listeners.end();
			 ++it2)
		{
			if (change.type == BT_GESTURE)
			{
				if(!(*it2)->OnDeviceButtonGesture(change.device, change.deviceButton, change.g))
				{
					break;
				}
			}
		}
	}
	
	for (Array<Change>::const_iterator it = changes_.begin();
			it != changes_.end();
			++it)
	{
		const Change& change = *it;
		for (Array<InputListener*>::iterator it2 = listeners.begin();
				it2 != listeners.end();
				++it2)
		{
			if (change.type == BT_BOOL)
			{
				if (!(*it2)->OnDeviceButtonBool(change.device, change.deviceButton, change.oldValue.b, change.newValue.b))
				{
					break;
				}
			}
			else if (change.type == BT_FLOAT)
			{
				if(!(*it2)->OnDeviceButtonFloat(change.device, change.deviceButton, change.oldValue.f, change.newValue.f))
				{
					break;
				}
			}
		}
	}
}

}

