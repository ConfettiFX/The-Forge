#pragma once

#include "../Components/PositionComponent.h"
#include "../../../../../Middleware_3/ECS/ComponentRepresentation.h"

FORGE_START_GENERATE_COMPONENT_REPRESENTATION(PositionComponent)

FORGE_REGISTER_COMPONENT_VAR(x)
FORGE_REGISTER_COMPONENT_VAR(y)

FORGE_END_GENERATE_COMPONENT_REPRESENTATION