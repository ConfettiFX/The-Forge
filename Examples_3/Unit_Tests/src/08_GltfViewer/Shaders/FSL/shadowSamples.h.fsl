/*
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

#ifndef _SHADOW_SAMPLES_H
#define _SHADOW_SAMPLES_H

#ifdef TARGET_ANDROID
	#define NUM_SHADOW_SAMPLES 4
#else
	#define NUM_SHADOW_SAMPLES 32
#endif

#if NUM_SHADOW_SAMPLES == 32
STATIC const float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
{
	-0.1746646, -0.7913184,
	-0.129792, -0.4477116,
	0.08863912, -0.898169,
	-0.5891499, -0.6781639,
	0.1748409, -0.5252063,
	0.6483325, -0.752117,
	0.4529319, -0.384986,
	0.09757467, -0.1166954,
	0.3857658, -0.9096935,
	0.5613058, -0.1283066,
	0.768011, -0.4906538,
	0.8499438, -0.220937,
	0.6946555, 0.1605866,
	0.9614297, 0.05975229,
	0.7986544, 0.5325912,
	0.4513965, 0.5592551,
	0.2847693, 0.2293397,
	-0.2118996, -0.1609127,
	-0.4357893, -0.3808875,
	-0.4662672, -0.05288446,
	-0.139129, 0.2394065,
	0.1781853, 0.5254948,
	0.4287854, 0.899425,
	0.1289349, 0.8724155,
	-0.6924323, -0.2203967,
	-0.48997, 0.2795907,
	-0.2611724, 0.7359962,
	-0.7704172, 0.4233134,
	-0.850104, 0.1263935,
	-0.8345267, -0.4991361,
	-0.5380967, 0.6264234,
	-0.9769312, -0.1550569
};
#elif NUM_SHADOW_SAMPLES == 16
STATIC const float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
{
	-0.1746646, -0.7913184,
	0.08863912, -0.898169,
	0.1748409, -0.5252063,
	0.4529319, -0.384986,
	0.3857658, -0.9096935,
	0.768011, -0.4906538,
	0.6946555, 0.1605866,
	0.7986544, 0.5325912,
	0.2847693, 0.2293397,
	-0.4357893, -0.3808875,
	-0.139129, 0.2394065,
	0.4287854, 0.899425,
	-0.6924323, -0.2203967,
	-0.2611724, 0.7359962,
	-0.850104, 0.1263935,
	-0.5380967, 0.6264234
};
#elif NUM_SHADOW_SAMPLES == 4
STATIC const float shadowSamples[NUM_SHADOW_SAMPLES * 2] =
{
	-0.1746646, -0.7913184,
	0.3857658, -0.9096935,
	0.2847693, 0.2293397,
	-0.6924323, -0.2203967,
};
#endif

STATIC const float NUM_SHADOW_SAMPLES_INV = 1.0 / NUM_SHADOW_SAMPLES;

#endif // _SHADOW_SAMPLES_H
