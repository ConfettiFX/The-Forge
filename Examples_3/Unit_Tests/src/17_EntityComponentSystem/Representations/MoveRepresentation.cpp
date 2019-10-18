#include "MoveRepresentation.h"

#include "../../../../../Common_3/OS/Interfaces/IMemory.h"
using namespace FCR;

FORGE_DEFINE_COMPONENT_ID(MoveComponent)

FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(MoveComponent, velx, 0)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(MoveComponent, vely, 1)

FORGE_START_VAR_REPRESENTATIONS_BUILD(MoveComponent)
FORGE_INIT_COMPONENT_ID(MoveComponent)

FORGE_CREATE_VAR_REPRESENTATION(MoveComponent, velx)
FORGE_FINALIZE_VAR_REPRESENTATION(velx, "velx", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(MoveComponent, vely)
FORGE_FINALIZE_VAR_REPRESENTATION(vely, "vely", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_END_VAR_REPRESENTATIONS_BUILD(MoveComponent)



FORGE_START_VAR_REFERENCES(MoveComponent)

FORGE_ADD_VAR_REF(MoveComponent, velx, velx)
FORGE_ADD_VAR_REF(MoveComponent, vely, vely)

FORGE_END_VAR_REFERENCES
