
#ifndef GAINPUTINPUTRECORDER_H_
#define GAINPUTINPUTRECORDER_H_

#ifdef GAINPUT_ENABLE_RECORDER

namespace gainput
{

/// Records a sequence of button state changes.
/**
 * In order for input recording to be available, Gainput must have been built with
 * \c GAINPUT_ENABLE_RECORDER defined.
 */
class GAINPUT_LIBEXPORT InputRecorder
{
public:
	/// Initializes the recorder.
	/**
	 * \param manager The InputManager to receive button state changes from.
	 */
	InputRecorder(InputManager& manager);
	/// Destructs the recorder.
	~InputRecorder();

	/// Starts recording.
	/**
	 * Also clears the InputRecording that is being recorded to so that it's not possible
	 * to resume recording after stopping to record.
	 */
	void Start();
	/// Stops recording.
	void Stop();
	/// Returns if the recorder is currently recording.
	bool IsRecording() const { return isRecording_; }

	/// Adds a device to record the button state changes of.
	/**
	 * If no device is set, all devices are recorded.
	 * \param device The ID of the device to record.
	 */
	void AddDeviceToRecord(DeviceId device) { recordedDevices_[device] = true; }
	/// Returns if the given device should be recorded.
	/**
	 * \param device The ID of the device to check.
	 */
	bool IsDeviceToRecord(DeviceId device) { return recordedDevices_.empty() || recordedDevices_.count(device) > 0; }

	/// Returns the recording that is being recorded to, may be 0.
	InputRecording* GetRecording() { return recording_; }
	/// Returns the recording that is being recorded to, may be 0.
	const InputRecording* GetRecording() const { return recording_; }
	/// Returns the time the recording was started.
	uint64_t GetStartTime() const { return startTime_; }

private:
	InputManager& manager_;

	bool isRecording_;
	InputListener* recordingListener_;
	ListenerId recordingListenerId_;
	InputRecording* recording_;
	uint64_t startTime_;
	HashMap<DeviceId, bool> recordedDevices_;

};

}

#endif

#endif

