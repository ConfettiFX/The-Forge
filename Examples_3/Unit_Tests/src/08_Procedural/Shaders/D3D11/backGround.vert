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

struct VSBGInput
{
	float4 vs_Pos : POSITION;
	float4 vs_Nor : NORMAL;
};

struct VSBGOutput {
	float4 Position : SV_POSITION;
	float2 fs_UV : TEXTURE0;
};

VSBGOutput main(VSBGInput input, uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID)
{
	VSBGOutput output = (VSBGOutput)0;

	if (VertexID == 0)
	{
		output.Position = float4(-1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 1.0);
	}
	else if (VertexID == 2)
	{
		output.Position = float4(1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 1.0);
	}
	else if (VertexID == 1)
	{
		output.Position = float4(1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 0.0);
	}
	else if (VertexID == 3)
	{
		output.Position = float4(-1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 1.0);
	}
	else if (VertexID == 5)
	{
		output.Position = float4(1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 0.0);
	}
	else if (VertexID == 4)
	{
		output.Position = float4(-1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 0.0);
	}

	return output;
}
