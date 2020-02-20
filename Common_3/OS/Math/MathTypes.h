/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#pragma once

#ifndef THEFORGE_INCLUDE_MATHTYPES_H
#define THEFORGE_INCLUDE_MATHTYPES_H
// ModifiedSonyMath ReadMe:
// - All you need to do is include the public header file vectormath.hpp. It will expose the relevant parts of
//   the library for you and try to select the SSE implementation if supported.
#include "../../ThirdParty/OpenSource/ModifiedSonyMath/vectormath.hpp"

typedef Vector2 vec2;
typedef Vector3 vec3;
typedef Vector4 vec4;

typedef IVector2 ivec2;
typedef IVector3 ivec3;
typedef IVector4 ivec4;

typedef UVector2 uvec2;
typedef UVector3 uvec3;
typedef UVector4 uvec4;

typedef Matrix2 mat2;
typedef Matrix3 mat3;
typedef Matrix4 mat4;
#endif // THEFORGE_INCLUDE_MATHTYPES_H
