
#ifndef GAINPUTINPUTDELTASTATE_H_
#define GAINPUTINPUTDELTASTATE_H_

namespace gainput
{

struct GestureChange
{
	unsigned type;
	// Average position of all touches
	float position[2];
	union
	{
		// Pan gesture data
		struct { float translation[2]; };
		// Pinch gesture data
		struct { float scale; float velocity; float distance[2]; };
		// Rotate gesture data
		struct { float rotation; };
	};
};
	
/// Stores a list of input state changes.
class GAINPUT_LIBEXPORT InputDeltaState
{
public:
	InputDeltaState(Allocator& allocator);

	/// Add a state change for a bool-type button.
	/**
	 * \param device The input device the change occurred on.
	 * \param deviceButton The input button that was changed.
	 * \param oldValue The old button state.
	 * \param newValue The new button state.
	 */
	void AddChange(DeviceId device, DeviceButtonId deviceButton, bool oldValue, bool newValue);
	/// Add a state change for a float-type button.
	/**
	 * \param device The input device the change occurred on.
	 * \param deviceButton The input button that was changed.
	 * \param oldValue The old button state.
	 * \param newValue The new button state.
	 */
	void AddChange(DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue);
	/// Add a state change for a gesture-type button.
	/**
	 * \param device The input device the change occurred on.
	 * \param deviceButton The input button that was changed.
	 * \param newValue The new gesture state.
	 */
	void AddChange(DeviceId device, DeviceButtonId deviceButton, const GestureChange& newValue);
	/// Clear list of state changes.
	void Clear();

	/// Notifies all input listeners of the previously recorded state changes.
	/**
	 * \param listeners A list of input listeners to notify.
	 */
	void NotifyListeners(Array<InputListener*>& listeners) const;

private:
	struct Change
	{
		DeviceId device;
		DeviceButtonId deviceButton;
		ButtonType type;
		union
		{
			struct 
			{
				union
				{
					bool b;
					float f;
				} oldValue, newValue;
			};
			GestureChange g;
		};
	};

	Array<Change> changes_;
	Array<Change> gestureChanges_;
};

}

#endif

