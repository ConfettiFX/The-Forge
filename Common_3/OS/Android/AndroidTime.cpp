#include <math.h>
#include <stdint.h>
#include <time.h>

/************************************************************************/
// Time Related Functions
/************************************************************************/

uint32_t getSystemTime()
{
	long            ms;    // Milliseconds
	time_t          s;     // Seconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	s = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6);    // Convert nanoseconds to milliseconds

	ms += s * 1000;

	return (uint32_t)ms;
}

int64_t getUSec()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * 1e6;
	return us;
}

uint32_t getTimeSinceStart() { return (uint32_t)time(NULL); }

int64_t getTimerFrequency()
{
	// This is us to s
	return 1000000LL;
}