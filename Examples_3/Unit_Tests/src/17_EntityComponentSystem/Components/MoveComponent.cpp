#include "MoveComponent.h"
#include "../Representations/MoveRepresentation.h"

#include "../../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be the last include in a cpp file

FORGE_IMPLEMENT_COMPONENT(MoveComponent)

// Based on: https://github.com/aras-p/dod-playground
static float RandomFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandomFloat(float from, float to) { return RandomFloat01() * (to - from) + from; }

MoveComponent::MoveComponent()
{

}

void MoveComponent::Initialize(float minSpeed, float maxSpeed)
{
	// random angle
	float angle = RandomFloat01() * 3.1415926f * 2;
	// random movement speed between given min & max
	float speed = RandomFloat(minSpeed, maxSpeed);
	// velocity x & y components
	velx = cosf(angle) * speed;
	vely = sinf(angle) * speed;
}