/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
*/

#define MAX_NUM_OBJECTS 128
#define MAX_NUM_PARTICLES 2048    // Per system
#define CUBES_EACH_ROW 5
#define CUBES_EACH_COL 5
#define CUBE_NUM (CUBES_EACH_ROW * CUBES_EACH_COL + 1)
#define DEBUG_OUTPUT 1       //exclusively used for texture data visulization, such as rendering depth, shadow map etc.
#if ((defined(DIRECT3D12) || defined(VULKAN) || defined(PROSPERO)) && !(defined(XBOX) || defined(QUEST_VR)) && !defined(ANDROID))
#define AOIT_ENABLE 1
#endif
#define AOIT_NODE_COUNT 4    // 2, 4 or 8. Higher numbers give better results at the cost of performance
#if AOIT_NODE_COUNT == 2
#define AOIT_RT_COUNT 1
#else
#define AOIT_RT_COUNT (AOIT_NODE_COUNT / 4)
#endif
#define USE_SHADOWS 1
#define PT_USE_REFRACTION 1
#define PT_USE_DIFFUSION 1
#define PT_USE_CAUSTICS (0 & USE_SHADOWS)

#define MAX_NUM_TEXTURES 7
