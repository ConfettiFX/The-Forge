#pragma once

#include "../../../../../Middleware_3/ECS/BaseComponent.h"

class PositionComponent : public BaseComponent {

	FORGE_DECLARE_COMPONENT(PositionComponent)

public:
	PositionComponent();

	float x, y;
};