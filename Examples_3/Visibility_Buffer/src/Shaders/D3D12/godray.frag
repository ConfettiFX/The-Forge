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

struct PsIn {
	float4 position: SV_Position;
	float2 texCoord: TEXCOORD;
};


Texture2D uTex0 : register(t0);
SamplerState uSampler0 : register(s0);

cbuffer RootConstantGodrayInfo : register(b0)
{
	float exposure;
	float decay;
	float density;
	float weight;

	float2 lightPosInSS;

	uint NUM_SAMPLES;
}


float4 main(PsIn input) : SV_Target
{		
	float2 deltaTexCoord = float2( input.texCoord - lightPosInSS.xy );
    float2 texCoord = input.texCoord;

	    
	deltaTexCoord *= 1.0 /  float(NUM_SAMPLES) * density;
    
	float illuminationDecay = 1.0;	
	float4 result = float4(0.0, 0.0, 0.0, 0.0);



    for(int i=0; i < NUM_SAMPLES ; i++)
    {
        texCoord -= deltaTexCoord;
        float4 color = uTex0.Sample(uSampler0, texCoord).rgba;
			
        color *= illuminationDecay *weight;
        result += color;
        illuminationDecay *= decay;
    }

    result *= exposure;
	return result;
}