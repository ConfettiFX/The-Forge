#pragma once

#include "../Components/MoveComponent.h"
#include "../../../../../Middleware_3/ECS/ComponentRepresentation.h"

FORGE_START_GENERATE_COMPONENT_REPRESENTATION(MoveComponent)

FORGE_REGISTER_COMPONENT_VAR(velx)
FORGE_REGISTER_COMPONENT_VAR(vely)

FORGE_END_GENERATE_COMPONENT_REPRESENTATION