/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
*/

#ifndef SKINNING_H
#define SKINNING_H

#ifndef MAX_NUM_BONES
#define MAX_NUM_BONES 200
#endif

CBUFFER(uniformBlock, UPDATE_FREQ_PER_DRAW, b0, binding = 0)
{
    DATA(float4x4, vpMatrix[VR_MULTIVIEW_COUNT], None);
    DATA(float4x4, modelMatrix, None);
};

CBUFFER(boneMatrices, UPDATE_FREQ_PER_DRAW, b1, binding = 1)
{
    DATA(float4x4, boneMatrix[MAX_NUM_BONES], None);
};

RES(Tex2D(float4), DiffuseTexture, UPDATE_FREQ_NONE, t0, binding = 3);
RES(SamplerState, DefaultSampler, UPDATE_FREQ_NONE, s0, binding = 4);

#endif
