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

Texture2D SourceTexture : register(t0, space1);
SamplerState repeatBillinearSampler : register(s1, space0);


float4 main(PsIn In) : SV_Target
{
	float4 sceneColor = SourceTexture.Sample(repeatBillinearSampler, In.texCoord);
	float3 resultColor = float3(0.0, 0.0, 0.0);
	resultColor = sceneColor.rgb;
	return float4(resultColor, 1.0);
}
