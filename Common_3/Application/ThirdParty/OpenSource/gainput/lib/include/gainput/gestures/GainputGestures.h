
#ifndef GAINPUTGESTURES_H_
#define GAINPUTGESTURES_H_

#ifdef GAINPUT_ENABLE_ALL_GESTURES
#define GAINPUT_ENABLE_BUTTON_STICK_GESTURE
#define GAINPUT_ENABLE_DOUBLE_CLICK_GESTURE
#define GAINPUT_ENABLE_HOLD_GESTURE
#define GAINPUT_ENABLE_PINCH_GESTURE
#define GAINPUT_ENABLE_ROTATE_GESTURE
#define GAINPUT_ENABLE_SIMULTANEOUSLY_DOWN_GESTURE
#define GAINPUT_ENABLE_TAP_GESTURE
#endif


namespace gainput
{

/// Base class for all input gestures.
/**
 * Input gestures are a way to process basic input data into more complex input data. For example, 
 * multiple buttons may be interpreted over time to form a new button. A very simple gesture would the
 * ubiquitous double-click.
 *
 * Mainly for consistency and convenience reasons, all gestures should derive from this class though it's not
 * strictly necessary (deriving from InputDevice would suffice).
 *
 * Input gestures are basically just input devices that don't get their data from some hardware device
 * but from other input devices instead. Therefore gestures must also be created by calling
 * InputManager::CreateDevice() or InputManager::CreateAndGetDevice(). Most gestures require further 
 * initialization which is done by calling one of their \c Initialize() member functions. After that, 
 * they should be good to go and their buttons can be used like any other input device button, i.e.
 * they can be mapped to some user button.
 *
 * Gestures can be excluded from compilation if they are not required. In order to include all gestures 
 * \c GAINPUT_ENABLE_ALL_GESTURES should be defined in \c gainput.h . This define must be present when
 * the library is built, otherwise the actual functionality won't be present. Similarly, there is one
 * define for each gesture. The names of these are documented in the descriptions of the 
 * individual gesture classes. If no such define is defined, no gesture will be included.
 */
class GAINPUT_LIBEXPORT InputGesture : public InputDevice
{
public:
	/// Returns DT_GESTURE.
	DeviceType GetType() const { return DT_GESTURE; }
	const char* GetTypeName() const { return "gesture"; }
	bool IsLateUpdate() const { return true; }

protected:
	/// Gesture base constructor.
	InputGesture(InputManager& manager, DeviceId device, unsigned index) : InputDevice(manager, device, index == InputDevice::AutoIndex ? manager.GetDeviceCountByType(DT_GESTURE) : 0) { }

	DeviceState InternalGetState() const { return DS_OK; }

};

}


#include "GainputButtonStickGesture.h"
#include "GainputDoubleClickGesture.h"
#include "GainputHoldGesture.h"
#include "GainputPinchGesture.h"
#include "GainputRotateGesture.h"
#include "GainputSimultaneouslyDownGesture.h"
#include "GainputTapGesture.h"

#endif

