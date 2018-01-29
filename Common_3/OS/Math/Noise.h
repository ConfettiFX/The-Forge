/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#ifndef _NOISE_H_
#define _NOISE_H_

float noise1(const float x);
float noise2(const float x, const float y);
float noise3(const float x, const float y, const float z);

float turbulence2(const float x, const float y, float freq);
float turbulence3(const float x, const float y, const float z, float freq);

float tileableNoise1(const float x, const float w);
float tileableNoise2(const float x, const float y, const float w, const float h);
float tileableNoise3(const float x, const float y, const float z, const float w, const float h, const float d);

float tileableTurbulence2(const float x, const float y, const float w, const float h, float freq);
float tileableTurbulence3(const float x, const float y, const float z, const float w, const float h, const float d, float freq);

void initNoise();


#endif // _NOISE_H_
