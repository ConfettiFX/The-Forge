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

#ifndef SHADER_DEFS_H
#define SHADER_DEFS_H

STRUCT(AsteroidDynamic)
{
	DATA(float4x4, transform, None);
    DATA(uint, indexStart, None);
    DATA(uint, indexEnd, None);
    DATA(uint, padding[2], None);
};

STRUCT(AsteroidStatic)
{
	DATA(float4, rotationAxis, None);
	DATA(float4, surfaceColor, None);
    DATA(float4, deepColor, None);

    DATA(float, scale, None);
	DATA(float, orbitSpeed, None);
	DATA(float, rotationSpeed, None);

    DATA(uint, textureID, None);
    DATA(uint, vertexStart, None);
    DATA(uint, padding[3], None);
};

#endif