#pragma once

#include "../../../../../Middleware_3/ECS/BaseComponent.h"

class WorldBoundsComponent : public BaseComponent {

	FORGE_DECLARE_COMPONENT(WorldBoundsComponent)

public:
	WorldBoundsComponent();

	float xMin, xMax, yMin, yMax;
};