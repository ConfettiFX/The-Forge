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

#ifndef deferred_shader_pointlight_h
#define deferred_shader_pointlight_h

struct ArgData
{
#if SAMPLE_COUNT > 1
    texture2d_ms<float,access::read> gBufferColor;
    texture2d_ms<float,access::read> gBufferNormal;
    texture2d_ms<float,access::read> gBufferSpecular;
    texture2d_ms<float,access::read> gBufferSimulation;
    depth2d_ms<float,access::read> gBufferDepth;
#else
    texture2d<float,access::read> gBufferColor;
    texture2d<float,access::read> gBufferNormal;
    texture2d<float,access::read> gBufferSpecular;
    texture2d<float,access::read> gBufferSimulation ;
    depth2d<float,access::read> gBufferDepth;
#endif
	constant LightData* lights;
};

struct ArgDataPerFrame
{
    constant PerFrameConstants& uniforms;
};

#endif /* deferred_shader_pointlight_h */
