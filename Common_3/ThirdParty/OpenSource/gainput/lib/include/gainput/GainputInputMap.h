
#ifndef GAINPUTINPUTMAP_H_
#define GAINPUTINPUTMAP_H_

namespace gainput
{

class UserButton;

/// Type for filter functions that can be used to filter mapped float inputs.
typedef float (*FilterFunc_T)(float const value, void* userData);

/// Maps user buttons to device buttons.
/**
 * This is the interface that should be used to get input. You can have several maps that are used
 * simultaneously or use different ones depending on game state. The user button IDs have to be unique per input map.
 *
 * InputMap uses the provided InputManager to get devices inputs and process them into user-mapped inputs. After creating 
 * an InputMap, you should map some device buttons to user buttons (using MapBool() or MapFloat()). User buttons are identified 
 * by an ID provided by you. In order to ensure their uniqueness, it's a good idea to define an enum containing all your user buttons
 * for any given InputMap. It's of course possible to map multiple different device button to one user button.
 *
 * After a user button has been mapped, you can query its state by calling one of the several GetBool* and GetFloat* functions. The
 * result will depend on the mapped device button(s) and the policy (set using SetUserButtonPolicy()).
 */
class GAINPUT_LIBEXPORT InputMap
{
public:
	/// Initializes the map.
	/**
	 * \param manager The input manager used to get device inputs.
	 * \param name The name for the input map (optional). If a name is provided, it is copied to an internal buffer.
	 * \param allocator The allocator to be used for all memory allocations.
	 */
	InputMap(InputManager& manager, const char* name = 0, Allocator& allocator = GetDefaultAllocator());
	InputMap(InputManager* manager, const char* name = 0, Allocator& allocator = GetDefaultAllocator());
	/// Unitializes the map.
	~InputMap();

	/// Clears all mapped inputs.
	void Clear();

	/// Returns the input manager this input map uses.
	const InputManager& GetManager() const { return manager_; }
	/// Returns the map's name, if any.
	/**
	 * \return The map's name or 0 if no name was set.
	 */
	const char* GetName() const { return name_; }
	/// Returns the map's auto-generated ID (that should not be used outside of the library).
	unsigned GetId() const { return id_; }

	/// Maps a bool-type button.
	/**
	 * \param userButton The user ID for this mapping.
	 * \param device The device's ID of the device button to be mapped.
	 * \param deviceButton The ID of the device button to be mapped.
	 * \return true if the mapping was created.
	 */
	bool MapBool(UserButtonId userButton, DeviceId device, DeviceButtonId deviceButton);
	/// Maps a float-type button, possibly to a custom range.
	/**
	 * \param userButton The user ID for this mapping.
	 * \param device The device's ID of the device button to be mapped.
	 * \param deviceButton The ID of the device button to be mapped.
	 * \param min Optional minimum value of the mapped button.
	 * \param max Optional maximum value of the mapped button.
	 * \param filterFunc Optional filter functions that modifies the device button value.
	 * \param filterUserData Optional user data pointer that is passed to filterFunc.
	 * \return true if the mapping was created.
	 */
	bool MapFloat(UserButtonId userButton, DeviceId device, DeviceButtonId deviceButton,
			float min = 0.0f, float max = 1.0f,
			FilterFunc_T filterFunc = 0, void* filterUserData = 0);
	/// Removes all mappings for the given user button.
	void Unmap(UserButtonId userButton);
	/// Returns if the given user button has any mappings.
	bool IsMapped(UserButtonId userButton) const;
	
	/// Gets all device buttons mapped to the given user button.
	/**
	 * \param userButton The user button ID of the button to return all mappings for.
	 * \param[out] outButtons An array with maxButtonCount fields to receive the device buttons that are mapped.
	 * \param maxButtonCount The number of fields in outButtons.
	 * \return The number of device buttons written to outButtons.
	 */
	size_t GetMappings(UserButtonId userButton, DeviceButtonSpec* outButtons, size_t maxButtonCount) const;
	
	/// Policy for how multiple device buttons are summarized in one user button.
	enum UserButtonPolicy
	{
		UBP_FIRST_DOWN,		///< The first device buttons that is down (or not 0.0f) determines the result.
		UBP_MAX,		///< The maximum of all device button states is the result.
		UBP_MIN,		///< The minimum of all device button states is the result.
		UBP_AVERAGE		///< The average of all device button states is the result.
	};
	/// Sets how a user button handles inputs from multiple device buttons.
	/**
	 * \return true if the policy was set, false otherwise (i.e. the user button doesn't exist).
	 */
	bool SetUserButtonPolicy(UserButtonId userButton, UserButtonPolicy policy);

	/// Sets a dead zone for a float-type button.
	/**
	 * If a dead zone is set for a button anything less or equal to the given value will be treated
	 * as 0.0f. The absolute input value is used in order to determine if the input value falls within the dead
	 * zone (i.e. with a dead zone of 0.2f, both -0.1f and 0.1f will result in 0.0f).
	 *
	 * \param userButton The user button's ID.
	 * \param deadZone The dead zone to be set.
	 * \return true if the dead zone was set, false otherwise (i.e. the user button doesn't exist).
	 */
	bool SetDeadZone(UserButtonId userButton, float deadZone);

	/// Returns the bool state of a user button.
	bool GetBool(UserButtonId userButton) const;
	/// Returns if the user button is newly down.
	bool GetBoolIsNew(UserButtonId userButton) const;
	/// Returns the bool state of a user button from the previous frame.
	bool GetBoolPrevious(UserButtonId userButton) const;
	/// Returns if the user button has been released.
	bool GetBoolWasDown(UserButtonId userButton) const;

	/// Returns the float state of a user button.
	float GetFloat(UserButtonId userButton) const;
	/// Returns the float state of a user button from the previous frame.
	float GetFloatPrevious(UserButtonId userButton) const;
	/// Returns the delta between the previous and the current frame of the float state of the given user button.
	float GetFloatDelta(UserButtonId userButton) const;

	/// Gets the name of the device button mapped to the given user button.
	/**
	 * \param userButton ID of the user button.
	 * \param buffer A char-buffer to receive the button name.
	 * \param bufferLength Length of the buffer receiving the button name in bytes.
	 * \return The number of bytes written to buffer (includes the trailing \0).
	 */
	size_t GetUserButtonName(UserButtonId userButton, char* buffer, size_t bufferLength) const;

	/// Returns the user button ID the given device button is mapped to.
	/**
	 * This function iterates over all mapped buttons and therefore shouldn't be used in a performance critical
	 * situation.
	 *
	 * \param device The device's ID of the device button to be checked.
	 * \param deviceButton The ID of the device button to be checked.
	 * \return The user button ID the device button is mapped to or InvalidDeviceButtonId if the device button is not mapped.
	 */
	UserButtonId GetUserButtonId(DeviceId device, DeviceButtonId deviceButton) const;
	
	/// Registers a listener to be notified when a button state changes.
	/**
	 * If there are listeners registered, all input devices will have to record their state changes. This incurs extra runtime costs.
	 */
	ListenerId AddListener(MappedInputListener* listener);
	/// De-registers the given listener.
	void RemoveListener(ListenerId listenerId);
	/// Sorts the list of listeners which controls the order in which listeners are called.
	/**
	 * The order of listeners may be important as the functions being called to notify a listener of a state change can control if
	 * the state change will be passed to any consequent listeners. Call this function whenever listener priorites have changed. It
	 * is automatically called by AddListener() and RemoveListener().
	 */
	void ReorderListeners();

private:
	InputManager& manager_;
	char* name_;
	unsigned id_;
	Allocator& allocator_;

	typedef HashMap<UserButtonId, UserButton*> UserButtonMap;
	UserButtonMap userButtons_;
	UserButtonId nextUserButtonId_;

	HashMap<ListenerId, MappedInputListener*> listeners_;
	Array<MappedInputListener*> sortedListeners_;
	unsigned nextListenerId_;
	InputListener* managerListener_;
	ListenerId managerListenerId_;

	float GetFloatState(UserButtonId userButton, bool previous) const;

	UserButton* GetUserButton(UserButtonId userButton);
	const UserButton* GetUserButton(UserButtonId userButton) const;

	// Do not copy.
	InputMap(const InputMap &);
	InputMap& operator=(const InputMap &);

};

}

#endif

