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
