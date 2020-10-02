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

#include <metal_stdlib>
using namespace metal;
struct ExtendCameraData
{
	float4x4 viewMat;
	float4x4 projMat;
	float4x4 viewProjMat;
	float4x4 InvViewProjMat;

	float4 cameraWorldPos;
	float4 viewPortSize;
};

struct PropertiesData
{
	uint renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;
	float useNormalMap;

	float intensity;
	float useFadeEffect;
	float padding01;
	float padding02;
};

struct VSOutput {
	float4 Position [[position]];
	float2 uv;
};

struct CSData {
    sampler nearestSampler                  [[id(0)]];
    sampler bilinearSampler                 [[id(1)]];
    texture2d<float> SceneTexture           [[id(2)]];
    texture2d<float> SSRTexture             [[id(3)]];
};

struct CSDataPerFrame {
    constant ExtendCameraData& cbExtendCamera        [[id(0)]];
    constant PropertiesData& cbProperties            [[id(1)]];
};

fragment float4 stageMain(
    VSOutput input [[stage_in]],
    constant CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
    constant CSDataPerFrame& csDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{	
	float4 outColor;
	float4 ssrColor = csData.SSRTexture.sample(csData.nearestSampler, input.uv, 0);
	//return csData.SSRTexture.sample(csData.bilinearSampler, input.uv);

	
	if(csDataPerFrame.cbProperties.renderMode == 0)
	{
		outColor = csData.SceneTexture.sample(csData.bilinearSampler, input.uv);
                outColor = outColor / (outColor + float4(1.0f));

	        float gammaCorr = 1.0f / 2.2f;

	        outColor.r = pow(outColor.r, gammaCorr);
	        outColor.g = pow(outColor.g, gammaCorr);
	        outColor.b = pow(outColor.b, gammaCorr);
	        outColor.w = 1.0;
                return outColor;
	}		
	else if(csDataPerFrame.cbProperties.renderMode == 1)
	{
		outColor = float4(0.0, 0.0, 0.0, 0.0);
	}	
	
	if(csDataPerFrame.cbProperties.useHolePatching < 0.5)
	{
		outColor.w = 1.0;

		if(ssrColor.w > 0.0)
		{
			outColor = ssrColor;
		}
	}
	else if(ssrColor.w > 0.0)
	{
		float threshold = ssrColor.w;
		float minOffset = threshold;
		

		float4 neighborColor00 = csData.SSRTexture.sample(csData.nearestSampler, input.uv + float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 0.0), 0);
		float4 neighborColorB00 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv + float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 0.0), 0);
		if(neighborColor00.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor00.w);			
		}

		float4 neighborColor01 = csData.SSRTexture.sample(csData.nearestSampler, input.uv - float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 0.0), 0);
		float4 neighborColorB01 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv - float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 0.0), 0);
		if(neighborColor01.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor01.w);			
		}

		float4 neighborColor02 = csData.SSRTexture.sample(csData.nearestSampler, input.uv + float2(0.0, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		float4 neighborColorB02 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv + float2(0.0, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		if(neighborColor02.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor02.w);			
		}

		float4 neighborColor03 = csData.SSRTexture.sample(csData.nearestSampler, input.uv - float2(0.0, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		float4 neighborColorB03 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv - float2(0.0, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		if(neighborColor03.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor03.w);			
		}

		float4 neighborColor04 = csData.SSRTexture.sample(csData.nearestSampler, input.uv + float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		float4 neighborColorB04 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv + float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);

		float4 neighborColor05 = csData.SSRTexture.sample(csData.nearestSampler, input.uv + float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, -1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		float4 neighborColorB05 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv +float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, -1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);

		float4 neighborColor06 = csData.SSRTexture.sample(csData.nearestSampler, input.uv + float2(-1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		float4 neighborColorB06 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv + float2(-1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);

		float4 neighborColor07 = csData.SSRTexture.sample(csData.nearestSampler, input.uv - float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);
		float4 neighborColorB07 = csData.SSRTexture.sample(csData.bilinearSampler, input.uv - float2(1.0/csDataPerFrame.cbExtendCamera.viewPortSize.x, 1.0/csDataPerFrame.cbExtendCamera.viewPortSize.y), 0);

		bool bUseExpensiveHolePatching = csDataPerFrame.cbProperties.useExpensiveHolePatching > 0.5;

		if(bUseExpensiveHolePatching)
		{
			if(neighborColor04.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor04.w);			
			}

			if(neighborColor05.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor05.w);			
			}

			if(neighborColor06.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor06.w);			
			}

			if(neighborColor07.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor07.w);			
			}
		}

		float blendValue = 0.5;

		if(bUseExpensiveHolePatching)
		{
			if(minOffset == neighborColor00.w)
			{
					outColor =  mix(neighborColor00, neighborColorB00, blendValue);
			}
			else if(minOffset == neighborColor01.w)
			{
					outColor = mix(neighborColor01, neighborColorB01, blendValue);
			}
			else if(minOffset == neighborColor02.w)
			{
					outColor = mix(neighborColor02, neighborColorB02, blendValue);
			}
			else if(minOffset == neighborColor03.w)
			{
					outColor = mix(neighborColor03, neighborColorB03, blendValue);
			}
			else if(minOffset == neighborColor04.w)
			{
					outColor = mix(neighborColor04, neighborColorB04, blendValue);
			}
			else if(minOffset == neighborColor05.w)
			{
					outColor = mix(neighborColor05, neighborColorB05, blendValue);
			}
			else if(minOffset == neighborColor06.w)
			{
					outColor = mix(neighborColor06, neighborColorB06, blendValue);
			}
			else if(minOffset == neighborColor07.w)
			{
					outColor = mix(neighborColor07, neighborColorB07, blendValue);
			}
		}
		else
		{
			if(minOffset == neighborColor00.w)
			{
					outColor = mix(neighborColor00, neighborColorB00, blendValue);
			}
			else if(minOffset == neighborColor01.w)
			{
					outColor = mix(neighborColor01, neighborColorB01, blendValue);
			}
			else if(minOffset == neighborColor02.w)
			{
					outColor = mix(neighborColor02, neighborColorB02, blendValue);
			}
			else if(minOffset == neighborColor03.w)
			{
					outColor = mix(neighborColor03, neighborColorB03, blendValue);
			}
		}

		if(minOffset <= threshold)
			outColor.w = 1.0;
		else
			outColor.w = 0.0;
	}
	
	if(csDataPerFrame.cbProperties.renderMode == 3)
	{
		if(ssrColor.w <= 0.0)
			outColor = csData.SceneTexture.sample(csData.bilinearSampler, input.uv);
	}

	if(csDataPerFrame.cbProperties.renderMode == 2)
	{
		outColor = outColor * csDataPerFrame.cbProperties.intensity + csData.SceneTexture.sample(csData.bilinearSampler, input.uv);
	}
        outColor = outColor / (outColor + float4(1.0f));

	float gammaCorr = 1.0f / 2.2f;

	outColor.r = pow(outColor.r, gammaCorr);
	outColor.g = pow(outColor.g, gammaCorr);
	outColor.b = pow(outColor.b, gammaCorr);
	outColor.w = 1.0;

	return outColor;
}
