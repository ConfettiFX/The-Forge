/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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
#ifndef __cplusplus
#include "../../../../Common_3/Graphics/ShaderUtilities.h.fsl"
#endif 
STRUCT(ScreenShotParams)
{
	DATA(uint, saveAsHDR, None);
	DATA(uint, convertToSrgb, None);
	DATA(uint, flipRedBlue, None);
    DATA(uint, pad0, None);
};

// for low end iOS devices, do not use Argument buffers
BEGIN_SRT_NO_AB(SrtCopyCompData)
	BEGIN_SRT_SET(PerDraw)
		DECL_CBUFFER(PerDraw, CBUFFER(ScreenShotParams), gScreenShotConstants);
		DECL_RWBUFFER(PerDraw, RWByteBuffer, gOutputBuffer)
		DECL_TEXTURE(PerDraw, Tex2DArray(float4), gInputTextureArray)
		DECL_TEXTURE(PerDraw, Tex2D(float4), gInputTexture)
	END_SRT_SET(PerDraw)
END_SRT(SrtCopyCompData)

