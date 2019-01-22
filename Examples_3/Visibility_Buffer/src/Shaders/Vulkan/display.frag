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
#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif



layout(location = 0) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

struct PsIn
{
    vec4 position;
    vec2 texCoord;
};
layout(set = 0, binding = 0) uniform texture2D uTex0;
layout(set = 0, binding = 1) uniform sampler uSampler0;
layout(push_constant) uniform RootConstantSCurveInfo_Block
{
    float C1;
    float C2;
    float C3;
    float UseSCurve;
    float ScurveSlope;
    float ScurveScale;
    float linearScale;
    float pad0;
    uint outputMode;
}RootConstantSCurveInfo;

const vec3 PQ_N = vec3 (0.15930176);
const vec3 PQ_M = vec3 (78.84375);
const vec3 PQ_c1 = vec3 (0.8359375);
const vec3 PQ_c2 = vec3 (18.8515625);
const vec3 PQ_c3 = vec3 (18.6875);
vec3 L2PQ_float3(vec3 L)
{
    (L = pow(max(vec3 (0.0), (L / vec3 (10000.0))),vec3(PQ_N)));
    vec3 PQ = pow(max(vec3 (0.0), ((PQ_c1 + (PQ_c2 * L)) / (vec3 (1.0) + (PQ_c3 * L)))),vec3(PQ_M));
    return clamp(PQ, 0.0, 1.0);
}
vec3 Rec709ToRec2020(vec3 color)
{
    mat3 conversion = {{0.627402, 0.329292, 0.043306}, {0.069095, 0.9195440, 0.01136}, {0.016394, 0.08802800, 0.8955780}};
    return ((conversion)*(color));
}
vec3 ApplyDolbySCurve(vec3 Color, float Scale, float Slope)
{
    vec3 pow_in = pow(abs((clamp((Color).rgb, vec3(0.000061), vec3 (65504.0)) / vec3 (Scale))),vec3(Slope));
    return ((vec3 (RootConstantSCurveInfo.C1) + (vec3 (RootConstantSCurveInfo.C2) * pow_in)) / (vec3 (1) + (vec3 (RootConstantSCurveInfo.C3) * pow_in)));
}
vec4 HLSLmain(PsIn In)
{
    vec4 sceneColor = texture(sampler2D( uTex0, uSampler0), vec2((In).texCoord));
    vec3 resultColor = vec3(0.0, 0.0, 0.0);
    if((RootConstantSCurveInfo.outputMode == uint (0)))
    {
        (resultColor = (sceneColor).rgb);
    }
    else
    {
        if((RootConstantSCurveInfo.UseSCurve > 0.5))
        {
            (resultColor = L2PQ_float3(ApplyDolbySCurve(Rec709ToRec2020((sceneColor).rgb), RootConstantSCurveInfo.ScurveScale, RootConstantSCurveInfo.ScurveSlope)));
        }
        else
        {
            (resultColor = L2PQ_float3(min((Rec709ToRec2020((sceneColor).rgb) * vec3 (RootConstantSCurveInfo.linearScale)), vec3 (10000.0))));
        }
    }
    return vec4(resultColor, 1.0);
}
void main()
{
    PsIn In;
    In.position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    In.texCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(In);
    rast_FragData0 = result;
}
