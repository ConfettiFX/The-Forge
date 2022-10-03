
#include "catch.hpp"

#include <gainput/gainput.h>

using namespace gainput;

TEST_CASE("InputRecording/general", "")
{
	InputRecording recording;

	REQUIRE(recording.GetDuration() == 0);
	REQUIRE(recording.GetSerializedSize() > 0);

	recording.AddChange(100, 0, 0, true);

	REQUIRE(recording.GetDuration() == 100);

	RecordedDeviceButtonChange change;

	REQUIRE(!recording.GetNextChange(50, change));
	REQUIRE(recording.GetNextChange(100, change));
	REQUIRE(change.time == 100);
	REQUIRE(change.deviceId == 0);
	REQUIRE(change.buttonId == 0);
	REQUIRE(change.b == true);

	REQUIRE(!recording.GetNextChange(1000, change));

	recording.Reset();
	recording.AddChange(150, 5, 102, 0.8f);

	REQUIRE(!recording.GetNextChange(50, change));
	REQUIRE(recording.GetNextChange(101, change));
	REQUIRE(recording.GetNextChange(160, change));
	REQUIRE(change.time == 150);
	REQUIRE(change.deviceId == 5);
	REQUIRE(change.buttonId == 102);
	REQUIRE(change.f == 0.8f);
	REQUIRE(!recording.GetNextChange(1000, change));

	recording.Reset();
	recording.Clear();

	REQUIRE(recording.GetDuration() == 0);
	REQUIRE(!recording.GetNextChange(10000, change));
}

TEST_CASE("InputRecording/serialization", "")
{
	InputManager manager;
	const DeviceId deviceId = manager.CreateDevice<InputDeviceMouse>();
	InputRecording recording;

	recording.AddChange(500, deviceId, MouseButtonLeft, true);
	recording.AddChange(600, deviceId, MouseButtonMiddle, false);
	recording.AddChange(700, deviceId, MouseAxisX, 0.2f);

	REQUIRE(recording.GetSerializedSize() > 0);

	void* serializedData = malloc(recording.GetSerializedSize());
	recording.GetSerialized(manager, serializedData);

	InputRecording recording2(manager, serializedData, recording.GetSerializedSize());

	free(serializedData);

	REQUIRE(recording.GetSerializedSize() == recording2.GetSerializedSize());
	REQUIRE(recording.GetDuration() == recording2.GetDuration());

	RecordedDeviceButtonChange change, change2;

	for (unsigned t = 500; t <= 700; t += 100)
	{
		REQUIRE(recording.GetNextChange(t, change));
		REQUIRE(recording2.GetNextChange(t, change2));
		REQUIRE(change.time == change2.time);
		REQUIRE(change.deviceId == change2.deviceId);
		REQUIRE(change.buttonId == change2.buttonId);
		if (t == 700)
		{
			REQUIRE(change.f == change2.f);
		}
		else
		{
			REQUIRE(change.b == change2.b);
		}
	}
}

TEST_CASE("InputPlayer", "")
{
	InputManager manager;
	InputPlayer player(manager);

	REQUIRE(!player.GetRecording());

	InputRecording recording;;
	player.SetRecording(&recording);

	REQUIRE(player.GetRecording() == &recording);

	REQUIRE(!player.IsPlaying());
	player.Start();
	REQUIRE(player.IsPlaying());
	player.Stop();
	REQUIRE(!player.IsPlaying());
}

TEST_CASE("InputRecorder", "")
{
	InputManager manager;

	InputRecorder recorder(manager);

	REQUIRE(!recorder.IsRecording());
	recorder.Start();
	REQUIRE(recorder.IsRecording());
	recorder.Stop();
	REQUIRE(!recorder.IsRecording());

	REQUIRE(recorder.GetRecording());

	REQUIRE(recorder.IsDeviceToRecord(0));
	REQUIRE(recorder.IsDeviceToRecord(1));
	REQUIRE(recorder.IsDeviceToRecord(2358349));

	recorder.AddDeviceToRecord(0);
	REQUIRE(recorder.IsDeviceToRecord(0));
	REQUIRE(!recorder.IsDeviceToRecord(1));
	REQUIRE(!recorder.IsDeviceToRecord(2358349));
}

