/*
* Copyright (c) 2018-2019 Confetti Interactive Inc.
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

// This shader loads gBuffer data and shades the pixel.


#version 450 core

layout(set = 3/*per-frame update frequency*/, binding = 1) uniform objectUniformBlock
{
    mat4 WorldViewProjMat[SPHERE_NUM];
    mat4 WorldMat[SPHERE_NUM];
    vec4 WorldBoundingSphereRadius[SPHERE_NUM];
};

layout(set = 1, binding = 0) uniform cameraUniformBlock
{
    mat4 View;
    mat4 Project;
    mat4 ViewProject;
    mat4 InvView;
};
layout(set = 1, binding = 1) uniform lightUniformBlock
{
    mat4 lightViewProj;
    vec4 lightPosition;
    vec4 lightColor;
    vec4 lightUpVec;
    float lightRange;
};

layout(location = 0) flat out vec4 WorldObjectCenterAndRadius;

const vec3 vertexIn[] = {
    vec3(-1.0,-1.0,-1.0),
    vec3( 1.0,-1.0,-1.0),
    vec3(-1.0, 1.0,-1.0),
    vec3( 1.0, 1.0,-1.0),
    vec3(-1.0,-1.0, 1.0),
    vec3( 1.0,-1.0, 1.0),
    vec3(-1.0, 1.0, 1.0),
    vec3( 1.0, 1.0, 1.0)
};

float getSpecialCosine(in vec3 a, vec3 b, in vec3 planeNormal)
{
    b -= planeNormal*dot(b,planeNormal);
    // a's length shall always be 1.44 !
    return dot(a,b)*inversesqrt(2.0*dot(b,b));
}

void main(void)
{
    float radius = WorldBoundingSphereRadius[gl_InstanceIndex].y; // this is a "fake" widened raidus to account for penumbra
    vec3 objectWorldPos = WorldMat[gl_InstanceIndex][3].xyz;
    WorldObjectCenterAndRadius = vec4(objectWorldPos,WorldBoundingSphereRadius[gl_InstanceIndex].x);
    
    
    vec3 rightVec, upVec, forwardVec;
    // all of this is for projective lights (generate a billboard facing light)
    vec3 lightToCenterVec = objectWorldPos-lightPosition.xyz;
    float lightToCenterDist2 = dot(lightToCenterVec,lightToCenterVec);
    float lightToCenterDistRcp = inversesqrt(lightToCenterDist2);
    vec3 lightToCenterDir = lightToCenterVec*lightToCenterDistRcp;
    rightVec = normalize(cross(lightToCenterDir,lightUpVec.xyz));
    upVec = cross(rightVec,lightToCenterDir);
    // projective light special case end
    
    
    // this code rotates the cube so that the 1 to 3 faces which are closest to the camera draw first
    // (there are no 4 and 5 front facing cases as cube is always kept on-edge to camera in the light plane)
    // for this whole scheme to work the front face orientation, index buffer, vertex positions
    // and left/right handedness need to be indentical
    vec3 cameraToCenterVec = objectWorldPos-InvView[3].xyz;
    vec3 negOffsetToFrontPlane = lightToCenterDir*radius;
    vec3 reorientFactors = vec3(getSpecialCosine(rightVec+upVec,cameraToCenterVec,lightToCenterDir),
                                getSpecialCosine(rightVec-upVec,cameraToCenterVec,lightToCenterDir),
                                dot(lightToCenterVec-negOffsetToFrontPlane,cameraToCenterVec-negOffsetToFrontPlane));
    reorientFactors.yz = sign(reorientFactors.yz);
    reorientFactors.y *= sqrt(1.0-reorientFactors.x*reorientFactors.x);
    
    vec3 modelVertex = vertexIn[gl_VertexIndex&7];
    // TODO: Concatenate these matrices and come up with a closed form for a single mat3
    modelVertex.yz = mat2(reorientFactors.z,0.0,0.0,reorientFactors.z)*modelVertex.yz;
    modelVertex.xy =(reorientFactors.z<0.0 ?
                        mat2(reorientFactors.y, reorientFactors.x,-reorientFactors.x,reorientFactors.y):
                        mat2(reorientFactors.x,-reorientFactors.y,reorientFactors.y,reorientFactors.x)
                    )*modelVertex.xy;
    // rotation for polygon order end
    
    
    vec3 vertex = mat2x3(rightVec,upVec)*modelVertex.xy;
    
    
    // all of this is for projective lights (gaccount for spheres becoming ellipses in projective transform_
    float radius2 = radius*radius;
    vertex *= sqrt(radius2/(max(lightToCenterDist2,radius2)-radius2)+radius2);
    vertex *= 1.1; // TODO: better overestimation for spherical cap compensation
    // projective light special case end
    
    
    vertex += objectWorldPos;
    
    
    // all of this is for projective lights (extrude billboard away from light, with projection)
    forwardVec = (vertex-lightPosition.xyz)*lightToCenterDistRcp;
    // projective light special case end
    vertex += forwardVec*mix(-radius,lightRange,modelVertex.z>0.0);
    
    vec4 modelPos = vec4(vertex,1.0);
    gl_Position = ViewProject*modelPos;
}
