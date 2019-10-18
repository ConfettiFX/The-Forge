#include "PositionRepresentation.h"

#include "../../../../../Common_3/OS/Interfaces/IMemory.h"
using namespace FCR;

FORGE_DEFINE_COMPONENT_ID(PositionComponent)

FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(PositionComponent, x, 0)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(PositionComponent, y, 1)

FORGE_START_VAR_REPRESENTATIONS_BUILD(PositionComponent)
FORGE_INIT_COMPONENT_ID(PositionComponent)
FORGE_CREATE_VAR_REPRESENTATION(PositionComponent, x)
FORGE_FINALIZE_VAR_REPRESENTATION(x, "x", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(PositionComponent, y)
FORGE_FINALIZE_VAR_REPRESENTATION(y, "y", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_END_VAR_REPRESENTATIONS_BUILD(PositionComponent)



FORGE_START_VAR_REFERENCES(PositionComponent)

FORGE_ADD_VAR_REF(PositionComponent, x, x)
FORGE_ADD_VAR_REF(PositionComponent, y, y)

FORGE_END_VAR_REFERENCES
