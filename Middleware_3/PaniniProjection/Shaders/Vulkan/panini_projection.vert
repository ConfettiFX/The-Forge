#version 450 core

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

																				 
// http://shaunlebron.github.io/visualizing-projections/ | scroll down to stereographic projection
//
// The stereographic projection requires two cameras(C1 & C2 below). The centered camera first projects the image 
// onto the cylinderical or spherical screen, exactly like before. But instead of unrolling it onto 
// a flat frame, we use a second camera to project it onto a flat frame. This is known as a Panini 
// projection if we use a cylinder rather than a sphere.
//
//=================================================================================================	flat projection plane
//                   
//                                      ooo OOO OOO ooo
//                                  oOO                 OOo
//                              oOO                         OOo					Z
//                           oOO                               OOo				^
//                         oOO                                   OOo			|
//                       oOO                                       OOo			|	
//                      oOO                                         OOo			|
//                     oOO                                           OOo		+-------> X
//                    oOO                                             OOo
//                    oOO             Panini Frame's Origin           OOo		
//                    oOO                     * C1                    OOo
//                    oOO                     ^                       OOo
//                    oOO                     |                       OOo
//                     oOO                    |                      OOo
//                      oOO                   |                     OOo
//                       oOO              D Parameter              OOo
//                         oOO                |                  OOo
//                           oO               |                OOo
//                              oOO           |             OOo
//                                  oOO       v C2       OOo
//                                      ooo OOO OOO ooo
//
//
vec2 PaniniProjection(vec2 V, float d, float s)
{	// src: http://tksharpless.net/vedutismo/Pannini/panini.pdf
	//
	// The Cartesian coordinates of a point on the cylinder are 
	//	: (x, y, z) = (sinPhi, tanTheta, -cosPhi)
	//
	// The distance from projection center to view plane is 	
	//	: d + 1.0f
	//
	// The distance from projection center to the parallel plane containing the cylinder point is 
	//	: d + cosPhi
	//
	// Mapping from sphere (or cylinder in this case) to plane is
	//	: h = S * sinPhi
	//	: v = S * tanTheta
	// where S = (d+1)/(d+cosPhi);
    const float XZLength = sqrt(V.x * V.x + 1.0f);
    const float sinPhi   = V.x / XZLength;
    const float tanTheta = V.y / XZLength;
    const float cosPhi   = sqrt(1.0f - sinPhi * sinPhi);
    const float S = (d + 1.0f) / (d + cosPhi);
    return S * vec2(sinPhi, mix(tanTheta, tanTheta / cosPhi, s));
}

layout(location = 0) in	vec4 position;
layout(location = 0) out vec2 oTexcoord;
layout(push_constant, std430) uniform PaniniParameters
{
	// horizontal field of view in degrees
	float FoVH;

	// D parameter: Distance of projection's center from the Panini frame's origin.
	//				i.e. controls horizontal compression.
	//				D = 0.0f    : regular rectilinear projection
	//				D = 1.0f    : Panini projection
	//				D->Infinity : cylindrical orthographic projection
	float D;

	// S parameter: A scalar that controls 'Hard Vertical Compression' of the projection.
	//				Panini projection produces curved horizontal lines, which can feel
	//				unnatural. Vertical compression attempts to straighten those curved lines.
	//				S parameter works for FoVH < 180 degrees
	//				S = 0.0f	: No compression
	//				S = 1.0f	: Full straightening
	float S;

	// After Panini projection, we'll need to upscale to fit to screen
	float Scale;
} PaniniRootConstants;

vec2 PaniniProjectionScreenPosition(vec2 screenPosition)
{
	const float fovH	= PaniniRootConstants.FoVH * (3.14159265359 / 180.f);
    const float D		= PaniniRootConstants.D;
    const float S		= PaniniRootConstants.S;
    const float upscale	= PaniniRootConstants.Scale;

    const vec2 unproject = vec2(tan(0.5f * fovH));
    const vec2 project = vec2(1.0f / unproject);

	// unproject the screenspace position, get the viewSpace xy and use it as direction
    const vec2 viewDirection = screenPosition * unproject;
    const vec2 paniniPosition = PaniniProjection(viewDirection, D, S);

	// project & upscale the panini position 
	return paniniPosition * project * upscale;
}

void main()
{
	vec2 outPosition = position.xy;			// [-1, +1]
	oTexcoord = position.xy * 0.5 + 0.5;	// [ 0,  1]
	outPosition.xy = PaniniProjectionScreenPosition(outPosition);
	outPosition.y *= -1.0f;
	gl_Position = vec4(outPosition, 0, 1);
}
