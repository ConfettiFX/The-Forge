
#ifndef GAINPUTINPUTPLAYER_H_
#define GAINPUTINPUTPLAYER_H_

#ifdef GAINPUT_ENABLE_RECORDER

namespace gainput
{

/// Plays back a previously recorded sequence of device state changes.
/**
 * In order for input recording to be available, Gainput must have been built with
 * \c GAINPUT_ENABLE_RECORDER defined.
 */
class GAINPUT_LIBEXPORT InputPlayer : public DeviceStateModifier
{
public:
	/// Initializes the player.
	/**
	 * \param manager The manager to receive the device state changes.
	 * \param recording The recording to play, may be 0.
	 */
	InputPlayer(InputManager& manager, InputRecording* recording = 0);
	/// Destructs the player.
	~InputPlayer();

	/// Updates the player, called internally from InputManager::Update().
	void Update(InputDeltaState* delta);

	/// Starts the playback.
	/**
	 * A recording must have been provided before doing this, either through the
	 * constructor or SetRecording().
	 */
	void Start();
	/// Stops the Playback.
	void Stop();
	/// Returns if the player is currently playing.
	bool IsPlaying() const { return isPlaying_; }

	/// Sets the recording to play.
	void SetRecording(InputRecording* recording);
	/// Returns the currently set recording.
	InputRecording* GetRecording() { return recording_; }
	/// Returns the currently set recording.
	const InputRecording* GetRecording() const { return recording_; }

private:
	InputManager& manager_;

	bool isPlaying_;
	InputRecording* recording_;
	uint64_t startTime_;

	Array<DeviceId> devicesToReset_;

	ModifierId playingModifierId_;
};

}

#endif

#endif

