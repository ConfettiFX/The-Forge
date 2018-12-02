
#ifndef GAINPUTINPUTLISTENER_H_
#define GAINPUTINPUTLISTENER_H_

namespace gainput
{

/// Listener interface that allows to receive notifications when device button states change.
class GAINPUT_LIBEXPORT InputListener
{
public:
	virtual ~InputListener() { }

	/// Called when a bool-type button state changes.
	/**
	 * \param device The input device's ID the state change occurred on.
	 * \param deviceButton The ID of the device button that changed.
	 * \param oldValue Previous state of the button.
	 * \param newValue New state of the button.
	 * \return true if the button may be processed by listeners with a lower priority, false otherwise.
	 */
	virtual bool OnDeviceButtonBool(DeviceId device, DeviceButtonId deviceButton, bool oldValue, bool newValue) { GAINPUT_UNUSED(device); GAINPUT_UNUSED(deviceButton); GAINPUT_UNUSED(oldValue); GAINPUT_UNUSED(newValue); return true; }

	/// Called when a float-type button state changes.
	/**
	 * \param device The input device's ID the state change occurred on.
	 * \param deviceButton The ID of the device button that changed.
	 * \param oldValue Previous state of the button.
	 * \param newValue New state of the button.
	 * \return true if the button may be processed by listeners with a lower priority, false otherwise.
	 */
	virtual bool OnDeviceButtonFloat(DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue) { GAINPUT_UNUSED(device); GAINPUT_UNUSED(deviceButton); GAINPUT_UNUSED(oldValue); GAINPUT_UNUSED(newValue); return true; }
	
	/// Called when a gesture button state changes.
	/**
	 * \param device The input device's ID the state change occurred on.
	 * \param deviceButton The ID of the device button that changed.
	 * \param gesture New state of the button.
	 * \return true if the button may be processed by listeners with a lower priority, false otherwise.
	 */
	virtual bool OnDeviceButtonGesture(DeviceId device, DeviceButtonId deviceButton, const struct GestureChange& gesture) { GAINPUT_UNUSED(device); GAINPUT_UNUSED(deviceButton); GAINPUT_UNUSED(gesture); return true; }

	/// Returns the priority which influences the order in which listeners are called by InputManager.
	/**
	 * \sa InputManager::ReorderListeners()
	 */
	virtual int GetPriority() const { return 0; }
};


/// Listener interface that allows to receive notifications when user button states change.
class GAINPUT_LIBEXPORT MappedInputListener
{
public:
	virtual ~MappedInputListener() { }

	/// Called when a bool-type button state changes.
	/**
	 * \param userButton The user button's ID.
	 * \param oldValue Previous state of the button.
	 * \param newValue New state of the button.
	 * \return true if the button may be processed by listeners with a lower priority, false otherwise.
	 */
	virtual bool OnUserButtonBool(UserButtonId userButton, bool oldValue, bool newValue) { GAINPUT_UNUSED(userButton); GAINPUT_UNUSED(oldValue); GAINPUT_UNUSED(newValue); return true; }

	/// Called when a float-type button state changes.
	/**
	 * \param userButton The user button's ID.
	 * \param oldValue Previous state of the button.
	 * \param newValue New state of the button.
	 * \return true if the button may be processed by listeners with a lower priority, false otherwise.
	 */
	virtual bool OnUserButtonFloat(UserButtonId userButton, float oldValue, float newValue) { GAINPUT_UNUSED(userButton); GAINPUT_UNUSED(oldValue); GAINPUT_UNUSED(newValue); return true; }

	/// Returns the priority which influences the order in which listeners are called by InputMap.
	/**
	 * \sa InputMap::ReorderListeners()
	 */
	virtual int GetPriority() const { return 0; }

};

}

#endif

