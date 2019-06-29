#pragma once

#include "../../../../../Middleware_3/ECS/BaseComponent.h"

class SpriteComponent : public BaseComponent {

	FORGE_DECLARE_COMPONENT(SpriteComponent)

public:
	SpriteComponent();

	float colorR, colorG, colorB;
	int   spriteIndex;
	float scale;
};