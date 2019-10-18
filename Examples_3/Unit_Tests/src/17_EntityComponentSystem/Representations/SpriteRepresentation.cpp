#include "SpriteRepresentation.h"

#include "../../../../../Common_3/OS/Interfaces/IMemory.h"
using namespace FCR;

FORGE_DEFINE_COMPONENT_ID(SpriteComponent)

FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(SpriteComponent, colorR, 0)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(SpriteComponent, colorG, 1)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(SpriteComponent, colorB, 2)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(SpriteComponent, spriteIndex, 3)
FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(SpriteComponent, scale, 4)

FORGE_START_VAR_REPRESENTATIONS_BUILD(SpriteComponent)
FORGE_INIT_COMPONENT_ID(SpriteComponent)

FORGE_CREATE_VAR_REPRESENTATION(SpriteComponent, colorR)
FORGE_FINALIZE_VAR_REPRESENTATION(colorR, "colorR", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(SpriteComponent, colorG)
FORGE_FINALIZE_VAR_REPRESENTATION(colorG, "colorG", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(SpriteComponent, colorB)
FORGE_FINALIZE_VAR_REPRESENTATION(colorB, "colorB", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(SpriteComponent, spriteIndex)
FORGE_FINALIZE_VAR_REPRESENTATION(spriteIndex, "spriteIndex", ComponentVarType::INT, ComponentVarAccess::READ_WRITE)

FORGE_CREATE_VAR_REPRESENTATION(SpriteComponent, scale)
FORGE_FINALIZE_VAR_REPRESENTATION(scale, "scale", ComponentVarType::FLOAT, ComponentVarAccess::READ_WRITE)

FORGE_END_VAR_REPRESENTATIONS_BUILD(SpriteComponent)



FORGE_START_VAR_REFERENCES(SpriteComponent)

FORGE_ADD_VAR_REF(SpriteComponent, colorR, colorR)
FORGE_ADD_VAR_REF(SpriteComponent, colorG, colorG)
FORGE_ADD_VAR_REF(SpriteComponent, colorB, colorB)
FORGE_ADD_VAR_REF(SpriteComponent, spriteIndex, spriteIndex)
FORGE_ADD_VAR_REF(SpriteComponent, scale, scale)

FORGE_END_VAR_REFERENCES
