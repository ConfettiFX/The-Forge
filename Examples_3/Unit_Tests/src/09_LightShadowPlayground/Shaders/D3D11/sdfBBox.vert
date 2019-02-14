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

static const float3 vertexIn[] = { 
    float3(-1.0f, -1.0f, -1.0f), 
    float3( 1.0f, -1.0f, -1.0f), 
    float3(-1.0f,  1.0f, -1.0f), 
    float3( 1.0f,  1.0f, -1.0f), 
    float3(-1.0f, -1.0f,  1.0f), 
    float3( 1.0f, -1.0f,  1.0f), 
    float3(-1.0f,  1.0f,  1.0f), 
    float3( 1.0f,  1.0f,  1.0f) 
};

cbuffer objectUniformBlock
{
    row_major float4x4 WorldViewProjMat[26] : packoffset(c0);
    row_major float4x4 WorldMat[26] : packoffset(c104);
    float4 WorldBoundingSphereRadius[26] : packoffset(c208);
};

cbuffer lightUniformBlock : register(b1)
{
    row_major float4x4 lightViewProj : packoffset(c0);
    float4 lightPosition : packoffset(c4);
    float4 lightColor : packoffset(c5);
    float4 lightUpVec : packoffset(c6);
    float  lightRange : packoffset(c7);
};

cbuffer cameraUniformBlock : register(b0)
{
    row_major float4x4 View : packoffset(c0);
    row_major float4x4 Project : packoffset(c4);
    row_major float4x4 ViewProject : packoffset(c8);
    row_major float4x4 InvView : packoffset(c12);
};

struct VsIn
{
    uint VertexIndex : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
};

struct PsIn
{
    nointerpolation float4 WorldObjectCenterAndRadius : TEXCOORD0;
    float4 Position : SV_Position;
};

float getSpecialCosine(float3 a, inout float3 b, float3 planeNormal)
{
    b -= planeNormal * dot(b, planeNormal);
    return dot(a, b) * rsqrt(2.0f * dot(b, b));
}

PsIn main(VsIn input)
{
    float radius = WorldBoundingSphereRadius[input.InstanceIndex].y;
    float3 objectWorldPos = WorldMat[input.InstanceIndex][3].xyz;
    float4 WorldObjectCenterAndRadius = float4(objectWorldPos, WorldBoundingSphereRadius[input.InstanceIndex].x);

    // all of this is for projective lights (generate a billboard facing light)
    float3 lightToCenterVec = objectWorldPos - lightPosition.xyz;
    float lightToCenterDist2 = dot(lightToCenterVec, lightToCenterVec);
    float lightToCenterDistRcp = rsqrt(lightToCenterDist2);
    float3 lightToCenterDir = lightToCenterVec * lightToCenterDistRcp;
    float3 rightVec = normalize(cross(lightToCenterDir, lightUpVec.xyz));
    float3 upVec = cross(rightVec, lightToCenterDir);
    // projective light special case end

    // this code rotates the cube so that the 1 to 3 faces which are closest to the camera draw first
    // (there are no 4 and 5 front facing cases as cube is always kept on-edge to camera in the light plane)
    // for this whole scheme to work the front face orientation, index buffer, vertex positions
    // and left/right handedness need to be indentical
    float3 cameraToCenterVec = objectWorldPos - InvView[3].xyz;
    float3 negOffsetToFrontPlane = lightToCenterDir * radius;
    float3 reorientFactors = float3(getSpecialCosine(rightVec + upVec, cameraToCenterVec, lightToCenterDir), 
                                    getSpecialCosine(rightVec - upVec, cameraToCenterVec, lightToCenterDir), 
                                    dot(lightToCenterVec - negOffsetToFrontPlane, cameraToCenterVec - negOffsetToFrontPlane));
    reorientFactors.yz = sign(-reorientFactors.yz); // compensate for DX differing from Vulkan and its negative viewport
    reorientFactors.y *= sqrt(1.0f - reorientFactors.x * reorientFactors.x);

    float3 modelVertex = vertexIn[input.VertexIndex & 7];
    // TODO: Concatenate these matrices and come up with a closed form for a single mat3
    modelVertex.yz = mul(float2x2(float2(reorientFactors.z, 0.0f), float2(0.0f, reorientFactors.z)), modelVertex.yz);
    float2x2 rotMat;
    if (reorientFactors.z < 0.0f)
        rotMat = float2x2(float2(reorientFactors.y, reorientFactors.x), float2(-reorientFactors.x, reorientFactors.y));
    else
        rotMat = float2x2(float2(reorientFactors.x, -reorientFactors.y), float2(reorientFactors.y, reorientFactors.x));

    modelVertex.xy = mul(rotMat, modelVertex.xy);
    // rotation for polygon order end
    
    float3 vertex = mul(float3x2(rightVec.x, upVec.x, rightVec.y, upVec.y, rightVec.z, upVec.z), modelVertex.xy);
    
    // all of this is for projective lights (gaccount for spheres becoming ellipses in projective transform_
    float radius2 = radius*radius;
    vertex *= sqrt(radius2/(max(lightToCenterDist2,radius2)-radius2)+radius2);
    vertex *= 1.1; // TODO: better overestimation for spherical cap compensation
    // projective light special case end

    vertex += objectWorldPos;

    // all of this is for projective lights (extrude billboard away from light, with projection)
    float3 forwardVec = (vertex - lightPosition.xyz) * lightToCenterDistRcp;
    // projective light special case end
    vertex += (forwardVec * ((modelVertex.z > 0.0f) ? lightRange : (-radius)));

    float4 modelPos = float4(vertex, 1.0f);
	
    PsIn output;
    output.Position = mul(modelPos, ViewProject);
    output.WorldObjectCenterAndRadius = WorldObjectCenterAndRadius;
    return output;
}
