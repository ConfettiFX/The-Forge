#version 450 core

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

#define FLT_MAX  3.402823466e+38F



layout(set = 0, binding = 0) uniform cbExtendCamera
{
	mat4 viewMat;
	mat4 projMat;
	mat4 viewProjMat;
	mat4 InvViewProjMat;

	vec4 cameraWorldPos;
	vec4 viewPortSize;
};

layout(binding = 1) uniform texture2D SceneTexture;
layout(binding = 2) uniform texture2D SSRTexture;

layout(set = 0, binding = 3) uniform cbProperties
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


layout(set = 0, binding = 4) uniform sampler nearestSampler;
layout(set = 0, binding = 5) uniform sampler bilinearSampler;



layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main()
{	
	vec4 ssrColor = texture(sampler2D(SSRTexture, nearestSampler), uv);
	
	
	
	if(renderMode == 0)
	{
		outColor = texture(sampler2D(SceneTexture, bilinearSampler), uv);	
		return;
	}		
	else if(renderMode == 1)
	{
		outColor = vec4(0.0);
	}	
	

	
	if(useHolePatching < 0.5)
	{
		outColor.w = 1.0;

		if(ssrColor.w > 0.0)
		{
			outColor = ssrColor;
			//outColor *= intensity;	
		}
	}
	else if(ssrColor.w > 0.0)
	{
		float threshold = ssrColor.w;
		float minOffset = threshold;
		

		vec4 neighborColor00 = texture(sampler2D(SSRTexture, nearestSampler), uv + vec2(1.0/viewPortSize.x, 0.0));
		vec4 neighborColorB00 = texture(sampler2D(SSRTexture, bilinearSampler), uv + vec2(1.0/viewPortSize.x, 0.0));
		if(neighborColor00.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor00.w);			
		}

		vec4 neighborColor01 = texture(sampler2D(SSRTexture, nearestSampler), uv - vec2(1.0/viewPortSize.x, 0.0));
		vec4 neighborColorB01 = texture(sampler2D(SSRTexture, bilinearSampler), uv - vec2(1.0/viewPortSize.x, 0.0));
		if(neighborColor01.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor01.w);			
		}

		vec4 neighborColor02 = texture(sampler2D(SSRTexture, nearestSampler), uv + vec2(0.0, 1.0/viewPortSize.y));
		vec4 neighborColorB02 = texture(sampler2D(SSRTexture, bilinearSampler), uv + vec2(0.0, 1.0/viewPortSize.y));
		if(neighborColor02.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor02.w);			
		}

		vec4 neighborColor03 = texture(sampler2D(SSRTexture, nearestSampler), uv - vec2(0.0, 1.0/viewPortSize.y));
		vec4 neighborColorB03 = texture(sampler2D(SSRTexture, bilinearSampler), uv - vec2(0.0, 1.0/viewPortSize.y));
		if(neighborColor03.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor03.w);			
		}

		vec4 neighborColor04 = texture(sampler2D(SSRTexture, nearestSampler), uv + vec2(1.0/viewPortSize.x, 1.0/viewPortSize.y));
		vec4 neighborColorB04 = texture(sampler2D(SSRTexture, bilinearSampler), uv + vec2(1.0/viewPortSize.x, 1.0/viewPortSize.y));


		vec4 neighborColor05 = texture(sampler2D(SSRTexture, nearestSampler), uv + vec2(1.0/viewPortSize.x, -1.0/viewPortSize.y));
		vec4 neighborColorB05 = texture(sampler2D(SSRTexture, bilinearSampler), uv +vec2(1.0/viewPortSize.x, -1.0/viewPortSize.y));


		vec4 neighborColor06 = texture(sampler2D(SSRTexture, nearestSampler), uv + vec2(-1.0/viewPortSize.x, 1.0/viewPortSize.y));
		vec4 neighborColorB06 = texture(sampler2D(SSRTexture, bilinearSampler), uv + vec2(-1.0/viewPortSize.x, 1.0/viewPortSize.y));


		vec4 neighborColor07 = texture(sampler2D(SSRTexture, nearestSampler), uv - vec2(1.0/viewPortSize.x, 1.0/viewPortSize.y));
		vec4 neighborColorB07 = texture(sampler2D(SSRTexture, bilinearSampler), uv - vec2(1.0/viewPortSize.x, 1.0/viewPortSize.y));


		bool bUseExpensiveHolePatching = useExpensiveHolePatching > 0.5;

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

		//outColor *= intensity;
		
		if(minOffset <= threshold)
			outColor.w = 1.0;
		else
			outColor.w = 0.0;


	}
	
	if(renderMode == 3)
	{
		if(ssrColor.w <= 0.0)
			outColor = texture(sampler2D(SceneTexture, bilinearSampler), uv);	
	}

	if(renderMode == 2)
	{
		outColor = outColor * intensity + texture(sampler2D(SceneTexture, bilinearSampler), uv);	
	}
	
}