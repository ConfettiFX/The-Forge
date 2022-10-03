
#ifndef GAINPUTINPUTRECORDING_H_
#define GAINPUTINPUTRECORDING_H_

#ifdef GAINPUT_ENABLE_RECORDER

namespace gainput
{

/// A single recorded change for a device button.
struct GAINPUT_LIBEXPORT RecordedDeviceButtonChange
{
	/// The time at which the change occurred.
	uint64_t time;
	/// The ID of the device owning the button that changed.
	DeviceId deviceId;
	/// The ID of the button that changed.
	DeviceButtonId buttonId;

	union
	{
		/// If the button's type is ::BT_BOOL, this contains the new value.
		bool b;
		/// If the button's type is ::BT_FLOAT, this contains the new value.
		float f;
	};
};

/// A recorded sequence of input changes.
/**
 * The recording can be recorded to, played, or serialized/deserialized.
 *
 * In order for input recording to be available, Gainput must have been built with
 * \c GAINPUT_ENABLE_RECORDER defined.
 *
 * \sa InputPlayer
 * \sa InputRecorder
 */
class GAINPUT_LIBEXPORT InputRecording
{
public:
	/// Initializes the recording in an empty state.
	/**
	 * \param allocator The allocator to be used for all memory allocations.
	 */
	InputRecording(Allocator& allocator = GetDefaultAllocator());
	/// Initializes the recording from the given serialized data.
	/**
	 * The recording is reconstructed from a previously serialized recording obtained through 
	 * GetSerialized().
	 *
	 * \param manager Used to resolve device and button references in the recording.
	 * \param data The serialized recording as obtained from GetSerialized().
	 * \param size The length of the serialized recording as obtained from GetSerializedSize().
	 * \param allocator The allocator to be used for all memory allocations.
	 */
	InputRecording(InputManager& manager, void* data, size_t size, Allocator& allocator = GetDefaultAllocator());

	/// Appends a device button change to the recording.
	/**
	 * The changes must be added in chronological order, i.e. time must always greater or equal to the time
	 * AddChange() was last called with.
	 *
	 * \param time The time elapsed before the change occurred.
	 * \param deviceId The ID of the device owning the button that changed.
	 * \param buttonId The ID of the button that changed.
	 * \param value The new value of the button.
	 */
	void AddChange(uint64_t time, DeviceId deviceId, DeviceButtonId buttonId, bool value);
	/// Appends a device button change to the recording.
	/**
	 * The changes must be added in chronological order, i.e. time must always greater or equal to the time
	 * AddChange() was last called with.
	 *
	 * \param time The time elapsed before the change occurred.
	 * \param deviceId The ID of the device owning the button that changed.
	 * \param buttonId The ID of the button that changed.
	 * \param value The new value of the button.
	 */
	void AddChange(uint64_t time, DeviceId deviceId, DeviceButtonId buttonId, float value);

	/// Removes all state changes.
	void Clear();

	/// Gets the next button change before and including the given time and returns it.
	/**
	 * \param time The time up to which to return changes.
	 * \param[out] outChange The change properties will be written to this if this function returns true.
	 * \return true if a change matching the given time was found, false otherwise.
	 */
	bool GetNextChange(uint64_t time, RecordedDeviceButtonChange& outChange);
	/// Resets the playback position.
	/**
	 * After calling this function, GetNextChange() will return changes from the beginning of the recorded
	 * sequence of changes again.
	 */
	void Reset() { position_ = 0; }

	/// Returns what time frame this recording spans.
	uint64_t GetDuration() const;

	/// Returns the size required to serialize this recording.
	size_t GetSerializedSize() const;
	/// Serializes this recording to the given buffer.
	/**
	 * This function serializes this recording so that it can be saved and read back in and deserialized later.
	 *
	 * \param manager Used to resolve device and button references in the recording.
	 * \param data A buffer of (at least) a size as returned by GetSerializedSize().
	 */
	void GetSerialized(InputManager& manager, void* data) const;

private:
	Array<RecordedDeviceButtonChange> changes_;

	/// The position in changes_ for reading.
	unsigned position_;
};

}

#endif

#endif

