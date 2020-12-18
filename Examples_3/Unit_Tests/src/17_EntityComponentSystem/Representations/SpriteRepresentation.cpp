/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

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
