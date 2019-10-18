#include "WorldBoundsRepresentation.h"

#include "../../../../../Common_3/OS/Interfaces/IMemory.h"
using namespace FCR;

FORGE_DEFINE_COMPONENT_ID(WorldBoundsComponent)

FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(WorldBoundsComponent, xMin, 0)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(WorldBoundsComponent, xMax, 1)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(WorldBoundsComponent, yMin, 2)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(WorldBoundsComponent, yMax, 3)

FORGE_START_VAR_REPRESENTATIONS_BUILD(WorldBoundsComponent)
FORGE_INIT_COMPONENT_ID(WorldBoundsComponent)

FORGE_CREATE_VAR_REPRESENTATION(WorldBoundsComponent, xMin)
FORGE_FINALIZE_VAR_REPRESENTATION(xMin, "xMin", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(WorldBoundsComponent, xMax)
FORGE_FINALIZE_VAR_REPRESENTATION(xMax, "xMax", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(WorldBoundsComponent, yMin)
FORGE_FINALIZE_VAR_REPRESENTATION(yMin, "yMin", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(WorldBoundsComponent, yMax)
FORGE_FINALIZE_VAR_REPRESENTATION(yMax, "yMax", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_END_VAR_REPRESENTATIONS_BUILD(WorldBoundsComponent)



FORGE_START_VAR_REFERENCES(WorldBoundsComponent)

FORGE_ADD_VAR_REF(WorldBoundsComponent, xMin, xMin)
FORGE_ADD_VAR_REF(WorldBoundsComponent, xMax, xMax)
FORGE_ADD_VAR_REF(WorldBoundsComponent, yMin, yMin)
FORGE_ADD_VAR_REF(WorldBoundsComponent, yMax, yMax)

FORGE_END_VAR_REFERENCES
