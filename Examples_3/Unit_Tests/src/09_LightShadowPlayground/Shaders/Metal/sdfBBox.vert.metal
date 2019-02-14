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
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    const float3 verts[8] = { float3((-1.0)), float3(1.0, (-1.0), (-1.0)), float3((-1.0), 1.0, (-1.0)), float3(1.0, 1.0, (-1.0)), float3((-1.0), (-1.0), 1.0), float3(1.0, (-1.0), 1.0), float3((-1.0), 1.0, 1.0), float3(1.0) };
    struct Uniforms_objectUniformBlock
    {
        float4x4 WorldViewProjMat[26];
        float4x4 WorldMat[26];
        float4 WorldBoundingSphereRadius[26];
    };
    constant Uniforms_objectUniformBlock & objectUniformBlock;
    struct Uniforms_lightUniformBlock
    {
        float4x4 lightViewProj;
        float4 lightPosition;
        float4 lightColor;
        float4 lightUpVec;
        float lightRange;
    };
    constant Uniforms_lightUniformBlock & lightUniformBlock;
    struct Uniforms_cameraUniformBlock
    {
		float4x4 View;
		float4x4 Project;
		float4x4 ViewProject;
		float4x4 InvView;
		float4x4 InvProj;
		float4x4 InvViewProject;
		float4 NDCConversionConstants[2];
		float Near;
		float FarNearDiff;
		float FarNear;
    };
    constant Uniforms_cameraUniformBlock & cameraUniformBlock;
    struct VsIn
    {
		uint VertexIndex;
        uint InstanceIndex;
    };
    struct PsIn
    {
        float4 WorldObjectCenterAndRadius [[flat]];
        float4 Position [[position]];
    };
    float getSpecialCosine(float3 a, thread float3(& b), float3 planeNormal)
    {
        (b -= (planeNormal * (float3)(dot(b, planeNormal))));
        return (dot(a, b) * rsqrt((2.0 * dot(b, b))));
    };
    void vert_main()
    {
    };
    PsIn main(VsIn input)
    {
		PsIn output;
		float radius = objectUniformBlock.WorldBoundingSphereRadius[input.InstanceIndex].y; // this is a "fake" widened raidus to account for penumbra
		float3 objectWorldPos = objectUniformBlock.WorldMat[input.InstanceIndex][3].xyz;
		output.WorldObjectCenterAndRadius = float4(objectWorldPos,objectUniformBlock.WorldBoundingSphereRadius[input.InstanceIndex].x);
		
		float3 rightVec, upVec, forwardVec;
		// all of this is for projective lights (generate a billboard facing light)
		float3 lightToCenterVec = objectWorldPos-lightUniformBlock.lightPosition.xyz;
		float lightToCenterDist2 = dot(lightToCenterVec,lightToCenterVec);
		float lightToCenterDistRcp = 1.0f / sqrt(lightToCenterDist2);
		float3 lightToCenterDir = lightToCenterVec*lightToCenterDistRcp;
		rightVec = normalize(cross(lightToCenterDir,lightUniformBlock.lightUpVec.xyz));
		upVec = cross(rightVec,lightToCenterDir);
		// projective light special case end
		
		
		// this code rotates the cube so that the 1 to 3 faces which are closest to the camera draw first
		// (there are no 4 and 5 front facing cases as cube is always kept on-edge to camera in the light plane)
		// for this whole scheme to work the front face orientation, index buffer, vertex positions
		// and left/right handedness need to be indentical
		float3 cameraToCenterVec = objectWorldPos-cameraUniformBlock.InvView[3].xyz;
		float3 negOffsetToFrontPlane = lightToCenterDir * radius;
		float3 reorientFactors = float3(getSpecialCosine(rightVec+upVec,cameraToCenterVec,lightToCenterDir),
									getSpecialCosine(rightVec-upVec,cameraToCenterVec,lightToCenterDir),
									dot(lightToCenterVec-negOffsetToFrontPlane,cameraToCenterVec-negOffsetToFrontPlane));
		reorientFactors.yz = sign(reorientFactors.yz);
		reorientFactors.y *= sqrt(1.0-reorientFactors.x*reorientFactors.x);
		
		float3 modelVertex = verts[input.VertexIndex & 7];
		// TODO: Concatenate these matrices and come up with a closed form for a single mat3
		modelVertex.yz = float2x2(reorientFactors.z,0.0,0.0,reorientFactors.z)*modelVertex.yz;
		modelVertex.xy =(reorientFactors.z<0.0 ?
						 float2x2(reorientFactors.y, reorientFactors.x,-reorientFactors.x,reorientFactors.y):
						 float2x2(reorientFactors.x,-reorientFactors.y,reorientFactors.y,reorientFactors.x)
						 )*modelVertex.xy;
		// rotation for polygon order end
		
		
		float3 vertexPos = float2x3(rightVec,upVec)*modelVertex.xy;
		
		
		// all of this is for projective lights (gaccount for spheres becoming ellipses in projective transform_
		float radius2 = radius*radius;
		vertexPos *= sqrt(radius2/(max(lightToCenterDist2,radius2)-radius2)+radius2);
		vertexPos *= 1.1; // TODO: better overestimation for spherical cap compensation
		// projective light special case end
		
		
		vertexPos += objectWorldPos;
		
		
		// all of this is for projective lights (extrude billboard away from light, with projection)
		forwardVec = (vertexPos-lightUniformBlock.lightPosition.xyz)*lightToCenterDistRcp;
		// projective light special case end
		vertexPos += forwardVec*mix(-radius,lightUniformBlock.lightRange,modelVertex.z>0.0);
		
		float4 modelPos = float4(vertexPos,1.0);
		output.Position = cameraUniformBlock.ViewProject*modelPos;
		
		return output;
    };

    Vertex_Shader(
constant Uniforms_objectUniformBlock & objectUniformBlock,constant Uniforms_lightUniformBlock & lightUniformBlock,constant Uniforms_cameraUniformBlock & cameraUniformBlock) :
objectUniformBlock(objectUniformBlock),lightUniformBlock(lightUniformBlock),cameraUniformBlock(cameraUniformBlock) {}
};


vertex Vertex_Shader::PsIn stageMain(
	uint VertexIndex [[vertex_id]],
	uint InstanceIndex [[instance_id]],
    constant Vertex_Shader::Uniforms_objectUniformBlock & objectUniformBlock [[buffer(0)]],
    constant Vertex_Shader::Uniforms_lightUniformBlock & lightUniformBlock [[buffer(1)]],
    constant Vertex_Shader::Uniforms_cameraUniformBlock & cameraUniformBlock [[buffer(2)]])
{
    Vertex_Shader::VsIn input0;
    input0.VertexIndex = VertexIndex;
    input0.InstanceIndex = InstanceIndex;
    Vertex_Shader main(
    objectUniformBlock,
    lightUniformBlock,
    cameraUniformBlock);
    return main.main(input0);
}
