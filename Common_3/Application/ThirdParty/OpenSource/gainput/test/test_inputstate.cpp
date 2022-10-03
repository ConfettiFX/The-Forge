
#include "catch.hpp"

#include <gainput/gainput.h>

using namespace gainput;

const unsigned ButtonCount = 15;

TEST_CASE("InputState", "")
{
	InputState state(GetDefaultAllocator(), ButtonCount);
	InputState state2(GetDefaultAllocator(), ButtonCount);

	for (unsigned i = 0; i < ButtonCount; ++i)
	{
		REQUIRE(!state.GetBool(i));
		REQUIRE(state.GetFloat(i) == 0.0f);
	}

	for (unsigned i = 0; i < ButtonCount; ++i)
	{
		const bool s = i & 1;
		state.Set(i, s);
		REQUIRE(state.GetBool(i) == s);
	}

	state2 = state;
	for (unsigned i = 0; i < ButtonCount; ++i)
	{
		REQUIRE(state2.GetBool(i) == state.GetBool(i));
	}

	float s = 0.0f;
	for (unsigned i = 0; i < ButtonCount; ++i)
	{
		state.Set(i, s);
		REQUIRE(state.GetFloat(i) == s);
		s += float(i)*0.0354f;
	}

	state2 = state;
	for (unsigned i = 0; i < ButtonCount; ++i)
	{
		REQUIRE(state2.GetFloat(i) == state.GetFloat(i));
	}
}

