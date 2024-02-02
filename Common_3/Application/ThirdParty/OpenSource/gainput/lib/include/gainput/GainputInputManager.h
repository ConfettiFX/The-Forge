
#ifndef GAINPUTINPUTMANAGER_H_
#define GAINPUTINPUTMANAGER_H_

#if defined(GAINPUT_PLATFORM_ANDROID)
struct ANativeActivity;
#endif

#define DEV_MOUSE____ 0U
#define DEV_RAW_MOUSE 1U
#define DEV_KEYBOARD_ 2U
#define DEV_TOUCH____ 3U
#define DEV_PAD_START 4U
#define DEV_PAD_END__ (DEV_PAD_START + MAX_INPUT_GAMEPADS)

namespace gainput
{

	typedef void(*DeviceListener)(void * metadata, DeviceId deviceId, InputDevice* device, bool doAdd);

/// Manages all input devices and some other helpful stuff.
/**
 * This manager takes care of all device-related things. Normally, you should only need one that contains
 * all your input devices.
 *
 * After instantiating an InputManager, you have to call SetDisplaySize(). You should also create some 
 * input devices using the template function CreateDevice() which returns the device ID that is needed 
 * to do anything further with the device (for example, see InputMap).
 *
 * The manager has to be updated every frame by calling Update(). Depending on the platform,
 * you may have to call another function as part of your message handling code (see HandleMessage(), HandleInput()).
 *
 * Note that destruction of an InputManager invalidates all input maps based on it and all devices created
 * through it.
 */
class GAINPUT_LIBEXPORT InputManager
{
public:
	/// Initializes the manager.
	/**
	 * Further initialization is typically necessary.
	 * \param allocator The memory allocator to be used for all allocations.
	 * \see SetDisplaySize
	 * \see GetTime
	 */
	InputManager(Allocator& allocator = GetDefaultAllocator());

	/**
	 * Initialize the input manager.
	 */
	void Init(void* windowInstance);

	/**
	 * Shutdown the input manager and free any memory it allocated.
	 */
	void Exit();

	/// Sets the window resolution.
	/**
	 * Informs the InputManager and its devices of the size of the window that is used for
	 * receiving inputs. For example, the size is used to to normalize mouse inputs.
	 */
	void SetDisplaySize(int width, int height) { mDisplayWidth = width; mDisplayHeight = height; }

#if defined(GAINPUT_PLATFORM_LINUX)
	/// [LINUX ONLY] Lets the InputManager handle the given X event.
	/**
	 * Call this function for event types MotionNotify, ButtonPress, 
	 * ButtonRelease, KeyPress, KeyRelease.
	 */
	void HandleEvent(XEvent& event);
#endif
#if defined(GAINPUT_PLATFORM_WIN)
	/// [WINDOWS ONLY] Lets the InputManager handle the given Windows message.
	/** 
	 * Call this function for message types WM_CHAR, WM_KEYDOWN, WM_KEYUP, 
	 * WM_SYSKEYDOWN, WM_SYSKEYUP, WM_?BUTTON*, WM_MOUSEMOVE, WM_MOUSEWHEEL.
	 */
	void HandleMessage(const MSG& msg);
#endif
#if defined(GAINPUT_PLATFORM_ANDROID)	
	/// [ANDROID ONLY] Lets the InputManager handle the given input event.
	int32_t HandleInput(AInputEvent* event, ANativeActivity* activity);

	struct DeviceInput
	{
		InputDevice::DeviceType deviceType;
		unsigned deviceIndex;
		ButtonType buttonType;
		DeviceButtonId buttonId;
		union
		{
			float f;
			bool b;
		} value;
	};
	void HandleDeviceInput(DeviceInput const& input);
#endif

	/// Updates the input state, call this every frame.
	void Update(float deltaTime);

	/**
	 * This is used to to clear all input states from all registered devices
	 */
	void ClearAllStates(gainput::DeviceId deviceId = gainput::InvalidDeviceId);

	/// Returns the allocator to be used for memory allocations.
	Allocator& GetAllocator() const { return mAllocator; }

	/// Returns a monotonic time in milliseconds.
	uint64_t GetTime() const;

	void SetDeviceListener(void* metadata, DeviceListener listener);
	void NotifyDeviceListener(DeviceId deviceId, InputDevice* device, bool doAdd);

	void CreateControllers(int cnt);

	DeviceId GetNextId() { return mNextDeviceID++; }

	/// Creates an input device and registers it with the manager.
	/**
	 * \tparam T The input device class, muste be derived from InputDevice.
	 * \param variant Requests the specified device variant. Note that this is only
	 * a request. Another implementation variant of the device may silently be instantiated
	 * instead. To determine what variant was instantiated, call InputDevice::GetVariant().
	 * \return The ID of the newly created input device.
	 */
	template<class T> DeviceId CreateDevice(unsigned index = InputDevice::AutoIndex, InputDevice::DeviceVariant variant = InputDevice::DV_STANDARD);
	
	/// Creates an input device, registers it with the manager and returns it.
	/**
	 * \tparam T The input device class, muste be derived from InputDevice.
	 * \param variant Requests the specified device variant. Note that this is only
	 * a request. Another implementation variant of the device may silently be instantiated
	 * instead. To determine what variant was instantiated, call InputDevice::GetVariant().
	 * \return The newly created input device.
	 */
	template<class T> T* CreateAndGetDevice(unsigned index = InputDevice::AutoIndex, InputDevice::DeviceVariant variant = InputDevice::DV_STANDARD);

	/// Remove the device from InputManager management
	/// DOES NOT DESTROY DEVICE!
	void RemoveDevice(DeviceId deviceId);

	/// Add the device back with the given ID
	void AddDevice(DeviceId deviceId, InputDevice* device);

	/// Returns the input device with the given ID.
	/**
	 * \return The input device or 0 if it doesn't exist.
	 */
	InputDevice* GetDevice(DeviceId deviceId);
	/// Returns the input device with the given ID.
	/**
	 * \return The input device or 0 if it doesn't exist.
	 */
	const InputDevice* GetDevice(DeviceId deviceId) const;
	/// Returns the ID of the device with the given type and index.
	/**
	 * \param typeName The name of the device type. Should come from InputDevice::GetTypeName().
	 * \param index The index of the device. Should come from InputDevice::GetIndex().
	 * \return The device's ID or InvalidDeviceId if no matching device exists.
	 */
	DeviceId FindDeviceId(const char* typeName, unsigned index) const;
	/// Returns the ID of the device with the given type and index.
	/**
	 * \param type The device type. Should come from InputDevice::GetType().
	 * \param index The index of the device. Should come from InputDevice::GetIndex().
	 * \return The device's ID or InvalidDeviceId if no matching device exists.
	 */
	DeviceId FindDeviceId(InputDevice::DeviceType type, unsigned index) const;

	typedef HashMap<DeviceId, InputDevice*> DeviceMap;
	/// Iterator over all registered devices.
	typedef DeviceMap::iterator iterator;
	/// Const iterator over all registered devices.
	typedef DeviceMap::const_iterator const_iterator;

	/// Returns the begin iterator over all registered devices.
	iterator begin() { return mDevices.begin(); }
	/// Returns the end iterator over all registered devices.
	iterator end() { return mDevices.end(); }
	/// Returns the begin iterator over all registered devices.
	const_iterator begin() const { return mDevices.begin(); }
	/// Returns the end iterator over all registered devices.
	const_iterator end() const { return mDevices.end(); }

	/// Registers a listener to be notified when a button state changes.
	/**
	 * If there are listeners registered, all input devices will have to record their state changes. This incurs extra runtime costs.
	 */
	ListenerId AddListener(InputListener* listener);
	/// De-registers the given listener.
	void RemoveListener(ListenerId listenerId);
	/// Sorts the list of listeners which controls the order in which listeners are called.
	/**
	 * The order of listeners may be important as the functions being called to notify a listener of a state change can control if
	 * the state change will be passed to any consequent listeners. Call this function whenever listener priorites have changed. It
	 * is automatically called by AddListener() and RemoveListener().
	 */
	void ReorderListeners();

	/// Checks if any button on any device is down.
	/**
	 * \param[out] outButtons An array with maxButtonCount fields to receive the device buttons that are down.
	 * \param maxButtonCount The number of fields in outButtons.
	 * \return The number of device buttons written to outButtons.
	 */
	size_t GetAnyButtonDown(DeviceButtonSpec* outButtons, size_t maxButtonCount) const;

	/// Returns the number of devices with the given type.
	unsigned GetDeviceCountByType(InputDevice::DeviceType type) const;
	/// Returns the graphical display's width in pixels.
	int GetDisplayWidth() const { return mDisplayWidth; }
	/// Returns the graphical display's height in pixels.
	int GetDisplayHeight() const { return mDisplayHeight; }

	/// Registers a modifier that will be called after devices have been updated.
	ModifierId AddDeviceStateModifier(DeviceStateModifier* modifier);
	/// De-registers the given modifier.
	void RemoveDeviceStateModifier(ModifierId modifierId);

	void EnqueueConcurrentChange(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, bool value);
	void EnqueueConcurrentChange(InputDevice& device, InputState& state, InputDeltaState* delta, DeviceButtonId buttonId, float value);

	/// [IN dev BUILDS ONLY] Connect to a remote host to send device state changes to.
	void ConnectForStateSync(const char* ip, unsigned port);
	/// [IN dev BUILDS ONLY] Initiate sending of device state changes to the given device.
	void StartDeviceStateSync(DeviceId deviceId);

	/// Enable/disable debug rendering of input devices.
	void SetDebugRenderingEnabled(bool enabled);
	/// Returns true if debug rendering is enabled, false otherwise.
	bool IsDebugRenderingEnabled() const { return mDebugRendererEnabled; }
	/// Sets the debug renderer to be used if debug rendering is enabled.
	void SetDebugRenderer(DebugRenderer* debugRenderer);
	/// Returns the previously set debug renderer.
	DebugRenderer* GetDebugRenderer() const { return pDebugRenderer; }

	//getter/setter for window Instance window instance 
	void SetWindowsInstance(void* instance) { pWindowInstance = instance; }
	void* GetWindowsInstance() { return pWindowInstance; }
    
    // Check whether HID is enabled.
	bool IsHIDEnabled() { return mHIDDiscoveryEnabled; }
private:
	Allocator& mAllocator;

	DeviceMap mDevices;
	unsigned mNextDeviceID;

	void* mDevListenerMetadata;
	DeviceListener mDevListener;
	int mPadCnt;
	Array<InputDevicePad*> mPads;
	Array<DeviceId> mToRemove;

	HashMap<ListenerId, InputListener*> mListeners;
	unsigned mNextListenerID;
	Array<InputListener*> mSortedListeners;

	HashMap<ModifierId, DeviceStateModifier*> mModifiers;
	unsigned mNextModifierID;

	InputDeltaState* pDeltaState;

	float mDeltaTimeRemainderMs;
	uint64_t mCurrentTime;
	struct Change
	{
		InputDevice* device;
		InputState* state;
		InputDeltaState* delta;
		DeviceButtonId buttonId;
		ButtonType type;
		union
		{
			bool b;
			float f;
		};
	};

	GAINPUT_CONC_QUEUE(Change) mConcurrentInputs;

	int mDisplayWidth;
	int mDisplayHeight;

	bool mDebugRendererEnabled;
	DebugRenderer* pDebugRenderer;
	void* pWindowInstance;
	bool mInitialized;

	void ApplyPendingDeletes();

	// Do not copy.
	InputManager(const InputManager &);
	InputManager& operator=(const InputManager &);
    
	bool mHIDDiscoveryEnabled = true;
public:
};


template<class T>
inline
DeviceId
InputManager::CreateDevice(unsigned index, InputDevice::DeviceVariant variant)
{
	unsigned id = GetNextId();
	T* device = mAllocator.New<T>(*this, id, index, variant);
	mDevices[id] = device;
	NotifyDeviceListener(id, device, true);
	return id;
}

template<class T>
inline
T*
InputManager::CreateAndGetDevice(unsigned index, InputDevice::DeviceVariant variant)
{
	unsigned id = GetNextId();
	T* device = mAllocator.New<T>(*this, id, index, variant);
	mDevices[id] = device;
	NotifyDeviceListener(id, device, true);
	return device;
}

inline
void
InputManager::RemoveDevice(DeviceId deviceId)
{
	if (deviceId == InvalidDeviceId ||
		mDevices.find(deviceId) == mDevices.end())
		return;

	NotifyDeviceListener(deviceId, mDevices[deviceId], false);
	// delay the removal so that update loops don't break
	mToRemove.push_back(deviceId);
}

inline
void
InputManager::AddDevice(DeviceId deviceId, InputDevice* device)
{
	if (deviceId == InvalidDeviceId ||
		device == NULL)
		return;

	NotifyDeviceListener(deviceId, device, true);
	mDevices[deviceId] = device;
}

inline
InputDevice*
InputManager::GetDevice(DeviceId deviceId)
{
	DeviceMap::iterator it = mDevices.find(deviceId);
	if (it == mDevices.end())
	{
		return 0;
	}
	return it->second;
}

inline
const InputDevice*
InputManager::GetDevice(DeviceId deviceId) const
{
	DeviceMap::const_iterator it = mDevices.find(deviceId);
	if (it == mDevices.end())
	{
		return 0;
	}
	return it->second;
}


/// Interface for modifiers that change device input states after they have been updated.
class DeviceStateModifier
{
public:
	/// Called after non-dependent devices have been updated.
	/**
	 * This function is called by InputManager::Update() after InputDevice::Update() has been
	 * called on all registered devices that have InputDevice::IsLateUpdate() return \c false.
	 *
	 * \param delta All device state changes should be registered with this delta state, may be 0.
	 */
	virtual void Update(InputDeltaState* delta) = 0;
};

}

#endif

