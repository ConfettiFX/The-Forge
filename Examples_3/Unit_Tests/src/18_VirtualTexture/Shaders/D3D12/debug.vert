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

struct VSInput
{
	float4 Position : POSITION;
	float4 Normal : NORMAL;
	float2 UV : TEXCOORD0;
};

struct VSOutput
{
  float4 Position : SV_POSITION;
	float2 outUV : TEXCOORD0;
};

VSOutput main(VSInput input, uint vertexID : SV_VertexID)
{
  VSOutput output;

  float4 position;
  position.zw = float2(0.0, 1.0);

  if(vertexID == 0)
  {
      position.x = -1.0;
      position.y = -1.0;
  }
  else if(vertexID == 1)
  {
      position.x = -1.0;
      position.y = 1.0;
  }
  else if(vertexID == 2)
  {
      position.x = 1.0;
      position.y = -1.0;
  }
  else if(vertexID == 3)
  {
      position.x = 1.0;
      position.y = -1.0;
  }
  else if(vertexID == 4)
  {
      position.x = -1.0;
      position.y = 1.0;
  }
  else
  {
      position.x = 1.0;
      position.y = 1.0;
  }

  output.Position = position;
  output.outUV = position.xy * float2(0.5, (-0.5)) + float2(0.5, 0.5);
  return output;
}