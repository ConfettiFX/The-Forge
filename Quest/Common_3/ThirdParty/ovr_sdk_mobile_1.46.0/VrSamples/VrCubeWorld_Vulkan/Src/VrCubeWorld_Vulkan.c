/************************************************************************************

Filename    :   VrCubeWorld_Vulkan.c
Content     :   This sample demonstrates how to use the Vulkan VrApi.
                This sample uses the Android NativeActivity class. This sample does
                not use the application framework.
                This sample only uses the VrApi.
Created     :   October, 2017
Authors     :   J.M.P. van Waveren, Gloria Kennickell

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>

#include "Framework_Vulkan.h"

#include "VrApi.h"
#include "VrApi_Vulkan.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#define DEBUG 1
#define OVR_LOG_TAG "VrCubeWorldVk"

#if !defined(ALOGE)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#endif

#if !defined(ALOGV)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#endif

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static ovrSampleCount SAMPLE_COUNT = OVR_SAMPLE_COUNT_4;

/*
    TODO:
    - enable layer for gles -> spir-v
*/

/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

/*
================================================================================

ovrGeometry

================================================================================
*/

static void ovrGeometry_CreateCube(ovrVkContext* context, ovrVkGeometry* geometry) {
    // The cube is centered about the origin and spans the [-1,1] XYZ range.
    static const ovrVector3f cubePositions[8] = {
        {-1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f}, // top
        {-1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, -1.0f} // bottom
    };

    static const ovrVector4f cubeColors[8] = {
        {1.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}, // top
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f} // bottom
    };

    static const ovrTriangleIndex cubeIndices[36] = {
        0, 2, 1, 2, 0, 3, // top
        4, 6, 5, 6, 4, 7, // bottom
        2, 6, 7, 7, 1, 2, // right
        0, 4, 5, 5, 3, 0, // left
        3, 5, 6, 6, 2, 3, // front
        0, 1, 7, 7, 4, 0 // back
    };

    ovrDefaultVertexAttributeArrays cubeAttributeArrays;
    ovrVkVertexAttributeArrays_Alloc(
        &cubeAttributeArrays.base,
        DefaultVertexAttributeLayout,
        8,
        OVR_VERTEX_ATTRIBUTE_FLAG_POSITION | OVR_VERTEX_ATTRIBUTE_FLAG_COLOR);

    for (int i = 0; i < 8; i++) {
        cubeAttributeArrays.position[i].x = cubePositions[i].x;
        cubeAttributeArrays.position[i].y = cubePositions[i].y;
        cubeAttributeArrays.position[i].z = cubePositions[i].z;
        cubeAttributeArrays.color[i].x = cubeColors[i].x;
        cubeAttributeArrays.color[i].y = cubeColors[i].y;
        cubeAttributeArrays.color[i].z = cubeColors[i].z;
        cubeAttributeArrays.color[i].w = cubeColors[i].w;
    }

    ovrVkTriangleIndexArray cubeIndexArray;
    ovrVkTriangleIndexArray_Alloc(&cubeIndexArray, 36, cubeIndices);

    ovrVkGeometry_Create(context, geometry, &cubeAttributeArrays.base, &cubeIndexArray);

    ovrVkVertexAttributeArrays_Free(&cubeAttributeArrays.base);
    ovrVkTriangleIndexArray_Free(&cubeIndexArray);
}

static void ovrGeometry_Destroy(ovrVkContext* context, ovrVkGeometry* geometry) {
    ovrVkGeometry_Destroy(context->device, geometry);
}

/*
================================================================================

ovrProgram

================================================================================
*/

enum {
    PROGRAM_UNIFORM_SCENE_MATRICES,
};

static ovrVkProgramParm colorOnlyProgramParms[] = {
    {OVR_PROGRAM_STAGE_FLAG_VERTEX,
     OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM,
     OVR_PROGRAM_PARM_ACCESS_READ_ONLY,
     PROGRAM_UNIFORM_SCENE_MATRICES,
     "SceneMatrices",
     0},
};

#define GLSL_VERSION "440 core" // maintain precision decorations: "310 es"
#define GLSL_EXTENSIONS                             \
    "#extension GL_EXT_shader_io_blocks : enable\n" \
    "#extension GL_ARB_enhanced_layouts : enable\n"

// clang-format off
static const char colorOnlyVertexProgramGLSL[] =
    "#version " GLSL_VERSION "\n"
    GLSL_EXTENSIONS
    "layout( std140, binding = 0 ) uniform SceneMatrices\n"
    "{\n"
    "	layout( offset =   0 ) mat4 ViewMatrix;\n"
    "	layout( offset =  64 ) mat4 ProjectionMatrix;\n"
    "};\n"
    "layout( location = 0 ) in vec3 vertexPosition;\n"
    "layout( location = 1 ) in vec4 vertexColor;\n"
    "layout( location = 2 ) in mat4 vertexTransform;\n"
    "layout( location = 0 ) out lowp vec4 fragmentColor;\n"
    "out gl_PerVertex { vec4 gl_Position; };\n"
    "void main( void )\n"
    "{\n"
    "	gl_Position = ProjectionMatrix * ( ViewMatrix * ( vertexTransform * vec4( vertexPosition * 0.1, 1.0 ) ) );\n"
    "	fragmentColor = vertexColor;\n"
    "}\n";

static const unsigned int colorOnlyVertexProgramSPIRV[] = {
	// 7.11.3057
	0x07230203,0x00010000,0x00080007,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000a,0x00000018,0x0000001c,
	0x0000002a,0x0000002c,0x00030003,0x00000002,0x000001b8,0x00070004,0x415f4c47,0x655f4252,
	0x6e61686e,0x5f646563,0x6f79616c,0x00737475,0x00070004,0x455f4c47,0x735f5458,0x65646168,
	0x6f695f72,0x6f6c625f,0x00736b63,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00060005,
	0x00000008,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000008,0x00000000,
	0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000000a,0x00000000,0x00060005,0x0000000e,
	0x6e656353,0x74614d65,0x65636972,0x00000073,0x00060006,0x0000000e,0x00000000,0x77656956,
	0x7274614d,0x00007869,0x00080006,0x0000000e,0x00000001,0x6a6f7250,0x69746365,0x614d6e6f,
	0x78697274,0x00000000,0x00030005,0x00000010,0x00000000,0x00060005,0x00000018,0x74726576,
	0x72547865,0x66736e61,0x006d726f,0x00060005,0x0000001c,0x74726576,0x6f507865,0x69746973,
	0x00006e6f,0x00060005,0x0000002a,0x67617266,0x746e656d,0x6f6c6f43,0x00000072,0x00050005,
	0x0000002c,0x74726576,0x6f437865,0x00726f6c,0x00050048,0x00000008,0x00000000,0x0000000b,
	0x00000000,0x00030047,0x00000008,0x00000002,0x00040048,0x0000000e,0x00000000,0x00000005,
	0x00050048,0x0000000e,0x00000000,0x00000023,0x00000000,0x00050048,0x0000000e,0x00000000,
	0x00000007,0x00000010,0x00040048,0x0000000e,0x00000001,0x00000005,0x00050048,0x0000000e,
	0x00000001,0x00000023,0x00000040,0x00050048,0x0000000e,0x00000001,0x00000007,0x00000010,
	0x00030047,0x0000000e,0x00000002,0x00040047,0x00000010,0x00000022,0x00000000,0x00040047,
	0x00000010,0x00000021,0x00000000,0x00040047,0x00000018,0x0000001e,0x00000002,0x00040047,
	0x0000001c,0x0000001e,0x00000000,0x00030047,0x0000002a,0x00000000,0x00040047,0x0000002a,
	0x0000001e,0x00000000,0x00040047,0x0000002c,0x0000001e,0x00000001,0x00020013,0x00000002,
	0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
	0x00000006,0x00000004,0x0003001e,0x00000008,0x00000007,0x00040020,0x00000009,0x00000003,
	0x00000008,0x0004003b,0x00000009,0x0000000a,0x00000003,0x00040015,0x0000000b,0x00000020,
	0x00000001,0x0004002b,0x0000000b,0x0000000c,0x00000000,0x00040018,0x0000000d,0x00000007,
	0x00000004,0x0004001e,0x0000000e,0x0000000d,0x0000000d,0x00040020,0x0000000f,0x00000002,
	0x0000000e,0x0004003b,0x0000000f,0x00000010,0x00000002,0x0004002b,0x0000000b,0x00000011,
	0x00000001,0x00040020,0x00000012,0x00000002,0x0000000d,0x00040020,0x00000017,0x00000001,
	0x0000000d,0x0004003b,0x00000017,0x00000018,0x00000001,0x00040017,0x0000001a,0x00000006,
	0x00000003,0x00040020,0x0000001b,0x00000001,0x0000001a,0x0004003b,0x0000001b,0x0000001c,
	0x00000001,0x0004002b,0x00000006,0x0000001e,0x3dcccccd,0x0004002b,0x00000006,0x00000020,
	0x3f800000,0x00040020,0x00000028,0x00000003,0x00000007,0x0004003b,0x00000028,0x0000002a,
	0x00000003,0x00040020,0x0000002b,0x00000001,0x00000007,0x0004003b,0x0000002b,0x0000002c,
	0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,
	0x00050041,0x00000012,0x00000013,0x00000010,0x00000011,0x0004003d,0x0000000d,0x00000014,
	0x00000013,0x00050041,0x00000012,0x00000015,0x00000010,0x0000000c,0x0004003d,0x0000000d,
	0x00000016,0x00000015,0x0004003d,0x0000000d,0x00000019,0x00000018,0x0004003d,0x0000001a,
	0x0000001d,0x0000001c,0x0005008e,0x0000001a,0x0000001f,0x0000001d,0x0000001e,0x00050051,
	0x00000006,0x00000021,0x0000001f,0x00000000,0x00050051,0x00000006,0x00000022,0x0000001f,
	0x00000001,0x00050051,0x00000006,0x00000023,0x0000001f,0x00000002,0x00070050,0x00000007,
	0x00000024,0x00000021,0x00000022,0x00000023,0x00000020,0x00050091,0x00000007,0x00000025,
	0x00000019,0x00000024,0x00050091,0x00000007,0x00000026,0x00000016,0x00000025,0x00050091,
	0x00000007,0x00000027,0x00000014,0x00000026,0x00050041,0x00000028,0x00000029,0x0000000a,
	0x0000000c,0x0003003e,0x00000029,0x00000027,0x0004003d,0x00000007,0x0000002d,0x0000002c,
	0x0003003e,0x0000002a,0x0000002d,0x000100fd,0x00010038
};

static const char colorOnlyMultiviewVertexProgramGLSL[] =
    "#version " GLSL_VERSION "\n"
    GLSL_EXTENSIONS
    "#extension GL_OVR_multiview2 : enable\n"
    "layout( std140, binding = 0 ) uniform SceneMatrices\n"
    "{\n"
    "	layout( offset =   0 ) mat4 ViewMatrix[2];\n"
    "	layout( offset =  128 ) mat4 ProjectionMatrix[2];\n"
    "};\n"
    "layout( location = 0 ) in vec3 vertexPosition;\n"
    "layout( location = 1 ) in vec4 vertexColor;\n"
    "layout( location = 2 ) in mat4 vertexTransform;\n"
    "layout( location = 0 ) out lowp vec4 fragmentColor;\n"
    "out gl_PerVertex { vec4 gl_Position; };\n"
    "void main( void )\n"
    "{\n"
    "	gl_Position = ProjectionMatrix[gl_ViewID_OVR] * ( ViewMatrix[gl_ViewID_OVR] * ( vertexTransform * vec4( vertexPosition * 0.1, 1.0 ) ) );\n"
    "	fragmentColor = vertexColor;\n"
    "}\n";

static const unsigned int colorOnlyMultiviewVertexProgramSPIRV[] = {
	// 7.11.3057
	0x07230203,0x00010000,0x00080007,0x00000036,0x00000000,0x00020011,0x00000001,0x00020011,
	0x00001157,0x0006000a,0x5f565053,0x5f52484b,0x746c756d,0x65697669,0x00000077,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x000b000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000a,0x00000017,0x00000020,
	0x00000024,0x00000032,0x00000034,0x00030003,0x00000002,0x000001b8,0x00070004,0x415f4c47,
	0x655f4252,0x6e61686e,0x5f646563,0x6f79616c,0x00737475,0x00070004,0x455f4c47,0x735f5458,
	0x65646168,0x6f695f72,0x6f6c625f,0x00736b63,0x00060004,0x4f5f4c47,0x6d5f5256,0x69746c75,
	0x77656976,0x00000032,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00060005,0x00000008,
	0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000008,0x00000000,0x505f6c67,
	0x7469736f,0x006e6f69,0x00030005,0x0000000a,0x00000000,0x00060005,0x00000012,0x6e656353,
	0x74614d65,0x65636972,0x00000073,0x00060006,0x00000012,0x00000000,0x77656956,0x7274614d,
	0x00007869,0x00080006,0x00000012,0x00000001,0x6a6f7250,0x69746365,0x614d6e6f,0x78697274,
	0x00000000,0x00030005,0x00000014,0x00000000,0x00060005,0x00000017,0x565f6c67,0x49776569,
	0x564f5f44,0x00000052,0x00060005,0x00000020,0x74726576,0x72547865,0x66736e61,0x006d726f,
	0x00060005,0x00000024,0x74726576,0x6f507865,0x69746973,0x00006e6f,0x00060005,0x00000032,
	0x67617266,0x746e656d,0x6f6c6f43,0x00000072,0x00050005,0x00000034,0x74726576,0x6f437865,
	0x00726f6c,0x00050048,0x00000008,0x00000000,0x0000000b,0x00000000,0x00030047,0x00000008,
	0x00000002,0x00040047,0x00000010,0x00000006,0x00000040,0x00040047,0x00000011,0x00000006,
	0x00000040,0x00040048,0x00000012,0x00000000,0x00000005,0x00050048,0x00000012,0x00000000,
	0x00000023,0x00000000,0x00050048,0x00000012,0x00000000,0x00000007,0x00000010,0x00040048,
	0x00000012,0x00000001,0x00000005,0x00050048,0x00000012,0x00000001,0x00000023,0x00000080,
	0x00050048,0x00000012,0x00000001,0x00000007,0x00000010,0x00030047,0x00000012,0x00000002,
	0x00040047,0x00000014,0x00000022,0x00000000,0x00040047,0x00000014,0x00000021,0x00000000,
	0x00040047,0x00000017,0x0000000b,0x00001158,0x00040047,0x00000020,0x0000001e,0x00000002,
	0x00040047,0x00000024,0x0000001e,0x00000000,0x00030047,0x00000032,0x00000000,0x00040047,
	0x00000032,0x0000001e,0x00000000,0x00040047,0x00000034,0x0000001e,0x00000001,0x00020013,
	0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,
	0x00000007,0x00000006,0x00000004,0x0003001e,0x00000008,0x00000007,0x00040020,0x00000009,
	0x00000003,0x00000008,0x0004003b,0x00000009,0x0000000a,0x00000003,0x00040015,0x0000000b,
	0x00000020,0x00000001,0x0004002b,0x0000000b,0x0000000c,0x00000000,0x00040018,0x0000000d,
	0x00000007,0x00000004,0x00040015,0x0000000e,0x00000020,0x00000000,0x0004002b,0x0000000e,
	0x0000000f,0x00000002,0x0004001c,0x00000010,0x0000000d,0x0000000f,0x0004001c,0x00000011,
	0x0000000d,0x0000000f,0x0004001e,0x00000012,0x00000010,0x00000011,0x00040020,0x00000013,
	0x00000002,0x00000012,0x0004003b,0x00000013,0x00000014,0x00000002,0x0004002b,0x0000000b,
	0x00000015,0x00000001,0x00040020,0x00000016,0x00000001,0x0000000e,0x0004003b,0x00000016,
	0x00000017,0x00000001,0x00040020,0x00000019,0x00000002,0x0000000d,0x00040020,0x0000001f,
	0x00000001,0x0000000d,0x0004003b,0x0000001f,0x00000020,0x00000001,0x00040017,0x00000022,
	0x00000006,0x00000003,0x00040020,0x00000023,0x00000001,0x00000022,0x0004003b,0x00000023,
	0x00000024,0x00000001,0x0004002b,0x00000006,0x00000026,0x3dcccccd,0x0004002b,0x00000006,
	0x00000028,0x3f800000,0x00040020,0x00000030,0x00000003,0x00000007,0x0004003b,0x00000030,
	0x00000032,0x00000003,0x00040020,0x00000033,0x00000001,0x00000007,0x0004003b,0x00000033,
	0x00000034,0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
	0x00000005,0x0004003d,0x0000000e,0x00000018,0x00000017,0x00060041,0x00000019,0x0000001a,
	0x00000014,0x00000015,0x00000018,0x0004003d,0x0000000d,0x0000001b,0x0000001a,0x0004003d,
	0x0000000e,0x0000001c,0x00000017,0x00060041,0x00000019,0x0000001d,0x00000014,0x0000000c,
	0x0000001c,0x0004003d,0x0000000d,0x0000001e,0x0000001d,0x0004003d,0x0000000d,0x00000021,
	0x00000020,0x0004003d,0x00000022,0x00000025,0x00000024,0x0005008e,0x00000022,0x00000027,
	0x00000025,0x00000026,0x00050051,0x00000006,0x00000029,0x00000027,0x00000000,0x00050051,
	0x00000006,0x0000002a,0x00000027,0x00000001,0x00050051,0x00000006,0x0000002b,0x00000027,
	0x00000002,0x00070050,0x00000007,0x0000002c,0x00000029,0x0000002a,0x0000002b,0x00000028,
	0x00050091,0x00000007,0x0000002d,0x00000021,0x0000002c,0x00050091,0x00000007,0x0000002e,
	0x0000001e,0x0000002d,0x00050091,0x00000007,0x0000002f,0x0000001b,0x0000002e,0x00050041,
	0x00000030,0x00000031,0x0000000a,0x0000000c,0x0003003e,0x00000031,0x0000002f,0x0004003d,
	0x00000007,0x00000035,0x00000034,0x0003003e,0x00000032,0x00000035,0x000100fd,0x00010038
};

static const char colorOnlyFragmentProgramGLSL[] =
    "#version " GLSL_VERSION "\n"
    GLSL_EXTENSIONS
    "layout( location = 0 ) in lowp vec4 fragmentColor;\n"
    "layout( location = 0 ) out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "	outColor = fragmentColor;\n"
    "}\n";

static const unsigned int colorOnlyFragmentProgramSPIRV[] = {
	// 7.11.3057
	0x07230203,0x00010000,0x00080007,0x0000000d,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001b8,0x00070004,0x415f4c47,0x655f4252,
	0x6e61686e,0x5f646563,0x6f79616c,0x00737475,0x00070004,0x455f4c47,0x735f5458,0x65646168,
	0x6f695f72,0x6f6c625f,0x00736b63,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
	0x00000009,0x4374756f,0x726f6c6f,0x00000000,0x00060005,0x0000000b,0x67617266,0x746e656d,
	0x6f6c6f43,0x00000072,0x00030047,0x00000009,0x00000000,0x00040047,0x00000009,0x0000001e,
	0x00000000,0x00030047,0x0000000b,0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000000,
	0x00030047,0x0000000c,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
	0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,
	0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,
	0x0000000a,0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,
	0x0000000c,0x0000000b,0x0003003e,0x00000009,0x0000000c,0x000100fd,0x00010038
};
// clang-format on

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int SwapChainLength;
    ovrTextureSwapChain* SwapChain;
    VkImage* ColorTextures;

    // These two fragment density pointers are null if not supported
    VkImage* FragmentDensityTextures;
    VkExtent2D* FragmentDensityTextureSizes;
} ovrColorSwapChain;

static bool ovrColorSwapChain_Init(
    ovrColorSwapChain* swapChain,
    const VkFormat colorFormat,
    const int width,
    const int height,
    bool isMultiview) {
    swapChain->SwapChain = vrapi_CreateTextureSwapChain3(
        isMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D,
        colorFormat,
        width,
        height,
        1,
        3);
    swapChain->SwapChainLength = vrapi_GetTextureSwapChainLength(swapChain->SwapChain);

    swapChain->ColorTextures = (VkImage*)malloc(swapChain->SwapChainLength * sizeof(VkImage));
    swapChain->FragmentDensityTextures =
        (VkImage*)malloc(swapChain->SwapChainLength * sizeof(VkImage));
    swapChain->FragmentDensityTextureSizes =
        (VkExtent2D*)malloc(swapChain->SwapChainLength * sizeof(VkExtent2D));

    for (int i = 0; i < swapChain->SwapChainLength; i++) {
        swapChain->ColorTextures[i] =
            vrapi_GetTextureSwapChainBufferVulkan(swapChain->SwapChain, i);
        if (swapChain->FragmentDensityTextures != NULL &&
            swapChain->FragmentDensityTextureSizes != NULL) {
            ovrResult result = vrapi_GetTextureSwapChainBufferFoveationVulkan(
                swapChain->SwapChain,
                i,
                &swapChain->FragmentDensityTextures[i],
                &swapChain->FragmentDensityTextureSizes[i].width,
                &swapChain->FragmentDensityTextureSizes[i].height);
            if (result != ovrSuccess) {
                free(swapChain->FragmentDensityTextures);
                free(swapChain->FragmentDensityTextureSizes);
                swapChain->FragmentDensityTextures = NULL;
                swapChain->FragmentDensityTextureSizes = NULL;
            }
        }
    }

    return true;
}

static bool ovrColorSwapChain_Destroy(ovrColorSwapChain* swapChain) {
    // Don't call vrapi destroy because the images are used by the framebuffer
    free(swapChain->ColorTextures);
    free(swapChain->FragmentDensityTextures);
    free(swapChain->FragmentDensityTextureSizes);
    return true;
}

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int Width;
    int Height;
    ovrSampleCount SampleCount;
    int TextureSwapChainLength;
    int TextureSwapChainIndex;
    ovrTextureSwapChain* ColorTextureSwapChain;
    ovrVkFramebuffer Framebuffer;
} ovrFrameBuffer;

static void ovrFramebuffer_Clear(ovrFrameBuffer* frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->SampleCount = OVR_SAMPLE_COUNT_1;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->ColorTextureSwapChain = NULL;

    memset(&frameBuffer->Framebuffer, 0, sizeof(ovrVkFramebuffer));
}

static bool ovrFramebuffer_Create(
    ovrVkContext* context,
    ovrFrameBuffer* frameBuffer,
    ovrVkRenderPass* renderPass,
    ovrColorSwapChain* swapChain,
    const VkFormat colorFormat,
    const int width,
    const int height,
    bool isMultiview) {
    assert(
        width >= 1 &&
        width <=
            (int)context->device->physicalDeviceProperties.properties.limits.maxFramebufferWidth);
    assert(
        height >= 1 &&
        height <=
            (int)context->device->physicalDeviceProperties.properties.limits.maxFramebufferHeight);

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->SampleCount = renderPass->sampleCount;
    frameBuffer->ColorTextureSwapChain = swapChain->SwapChain;
    frameBuffer->TextureSwapChainLength = swapChain->SwapChainLength;

    memset(&frameBuffer->Framebuffer, 0, sizeof(ovrVkFramebuffer));

    frameBuffer->Framebuffer.colorTextures =
        (ovrVkTexture*)malloc(frameBuffer->TextureSwapChainLength * sizeof(ovrVkTexture));
    if (swapChain->FragmentDensityTextures != NULL) {
        frameBuffer->Framebuffer.fragmentDensityTextures =
            (ovrVkTexture*)malloc(frameBuffer->TextureSwapChainLength * sizeof(ovrVkTexture));
    } else {
        frameBuffer->Framebuffer.fragmentDensityTextures = NULL;
    }
    frameBuffer->Framebuffer.framebuffers =
        (VkFramebuffer*)malloc(frameBuffer->TextureSwapChainLength * sizeof(VkFramebuffer));
    frameBuffer->Framebuffer.renderPass = renderPass;
    frameBuffer->Framebuffer.width = width;
    frameBuffer->Framebuffer.height = height;
    frameBuffer->Framebuffer.numLayers = isMultiview ? 2 : 1;
    frameBuffer->Framebuffer.numBuffers = frameBuffer->TextureSwapChainLength;
    frameBuffer->Framebuffer.currentBuffer = 0;
    frameBuffer->Framebuffer.currentLayer = 0;

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the ovrVkTexture from the texture swapchain.
        ovrVkTexture* texture = &frameBuffer->Framebuffer.colorTextures[i];
        texture->width = width;
        texture->height = height;
        texture->depth = 1;
        texture->layerCount = isMultiview ? 2 : 1;
        texture->mipCount = 1;
        texture->sampleCount = OVR_SAMPLE_COUNT_1;
        texture->usage = OVR_TEXTURE_USAGE_SAMPLED;
        texture->usageFlags = OVR_TEXTURE_USAGE_COLOR_ATTACHMENT | OVR_TEXTURE_USAGE_SAMPLED |
            OVR_TEXTURE_USAGE_STORAGE;
        texture->wrapMode =
            OVR_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER; // Uses VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
        texture->filter = OVR_TEXTURE_FILTER_LINEAR;
        texture->maxAnisotropy = 1.0f;
        texture->format = colorFormat;
        texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        texture->image = swapChain->ColorTextures[i];
        texture->memory = VK_NULL_HANDLE;
        texture->sampler = VK_NULL_HANDLE;
        texture->view = VK_NULL_HANDLE;

        {
            ovrVkContext_CreateSetupCmdBuffer(context);

            VkImageMemoryBarrier imageMemoryBarrier;
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.pNext = NULL;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image = swapChain->ColorTextures[i];
            imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
            imageMemoryBarrier.subresourceRange.levelCount = 1;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
            imageMemoryBarrier.subresourceRange.layerCount = texture->layerCount;

            const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            const VkDependencyFlags flags = 0;

            VC(context->device->vkCmdPipelineBarrier(
                context->setupCommandBuffer,
                src_stages,
                dst_stages,
                flags,
                0,
                NULL,
                0,
                NULL,
                1,
                &imageMemoryBarrier));

            ovrVkContext_FlushSetupCmdBuffer(context);
        }

        // create a view
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = NULL;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = texture->image;
        imageViewCreateInfo.viewType =
            isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = texture->format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = texture->layerCount;

        VK(context->device->vkCreateImageView(
            context->device->device, &imageViewCreateInfo, VK_ALLOCATOR, &texture->view));

        ovrVkTexture_UpdateSampler(context, texture);

        // Setup the foveation buffer texture if possible.
        if (frameBuffer->Framebuffer.fragmentDensityTextures != NULL) {
            // Create the ovrVkTexture from the texture swapchain. We don't actually need the rest
            // of the properties here, so we don't set them.
            ovrVkTexture* textureFragmentDensity =
                &frameBuffer->Framebuffer.fragmentDensityTextures[i];
            textureFragmentDensity->width = swapChain->FragmentDensityTextureSizes[i].width;
            textureFragmentDensity->height = swapChain->FragmentDensityTextureSizes[i].height;
            textureFragmentDensity->depth = 1;
            textureFragmentDensity->layerCount = isMultiview ? 2 : 1;
            textureFragmentDensity->mipCount = 1;
            textureFragmentDensity->sampleCount = OVR_SAMPLE_COUNT_1;
            textureFragmentDensity->usage = OVR_TEXTURE_USAGE_FRAG_DENSITY;
            textureFragmentDensity->wrapMode =
                OVR_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER; // Uses VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
            textureFragmentDensity->filter = OVR_TEXTURE_FILTER_LINEAR;
            textureFragmentDensity->maxAnisotropy = 1.0f;
            textureFragmentDensity->format = VK_FORMAT_R8G8_UNORM;
            textureFragmentDensity->imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
            textureFragmentDensity->image = swapChain->FragmentDensityTextures[i];
            textureFragmentDensity->memory = VK_NULL_HANDLE;
            textureFragmentDensity->sampler = VK_NULL_HANDLE;
            textureFragmentDensity->view = VK_NULL_HANDLE;

            {
                ovrVkContext_CreateSetupCmdBuffer(context);

                VkImageMemoryBarrier imageMemoryBarrier;
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.pNext = NULL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
                imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageMemoryBarrier.image = swapChain->FragmentDensityTextures[i];
                imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
                imageMemoryBarrier.subresourceRange.levelCount = 1;
                imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
                imageMemoryBarrier.subresourceRange.layerCount = textureFragmentDensity->layerCount;

                const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                const VkDependencyFlags flags = 0;

                VC(context->device->vkCmdPipelineBarrier(
                    context->setupCommandBuffer,
                    src_stages,
                    dst_stages,
                    flags,
                    0,
                    NULL,
                    0,
                    NULL,
                    1,
                    &imageMemoryBarrier));

                ovrVkContext_FlushSetupCmdBuffer(context);
            }

            // create a view
            VkImageViewCreateInfo imageViewCreateInfoFragmentDensity;
            imageViewCreateInfoFragmentDensity.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfoFragmentDensity.pNext = NULL;
            imageViewCreateInfoFragmentDensity.flags = 0;
            imageViewCreateInfoFragmentDensity.image = textureFragmentDensity->image;
            imageViewCreateInfoFragmentDensity.viewType =
                isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfoFragmentDensity.format = VK_FORMAT_R8G8_UNORM;
            imageViewCreateInfoFragmentDensity.components.r = VK_COMPONENT_SWIZZLE_R;
            imageViewCreateInfoFragmentDensity.components.g = VK_COMPONENT_SWIZZLE_G;
            imageViewCreateInfoFragmentDensity.components.b = VK_COMPONENT_SWIZZLE_B;
            imageViewCreateInfoFragmentDensity.components.a = VK_COMPONENT_SWIZZLE_A;
            imageViewCreateInfoFragmentDensity.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfoFragmentDensity.subresourceRange.baseMipLevel = 0;
            imageViewCreateInfoFragmentDensity.subresourceRange.levelCount = 1;
            imageViewCreateInfoFragmentDensity.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfoFragmentDensity.subresourceRange.layerCount =
                textureFragmentDensity->layerCount;

            VK(context->device->vkCreateImageView(
                context->device->device,
                &imageViewCreateInfoFragmentDensity,
                VK_ALLOCATOR,
                &textureFragmentDensity->view));

            ovrVkTexture_UpdateSampler(context, textureFragmentDensity);
        }
    }

    if (renderPass->sampleCount > OVR_SAMPLE_COUNT_1) {
        ovrVkTexture_Create2D(
            context,
            &frameBuffer->Framebuffer.renderTexture,
            (ovrVkTextureFormat)renderPass->internalColorFormat,
            renderPass->sampleCount,
            frameBuffer->Width,
            frameBuffer->Height,
            1,
            isMultiview ? 2 : 1,
            OVR_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ovrVkContext_CreateSetupCmdBuffer(context);
        ovrVkTexture_ChangeUsage(
            context,
            context->setupCommandBuffer,
            &frameBuffer->Framebuffer.renderTexture,
            OVR_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ovrVkContext_FlushSetupCmdBuffer(context);
    }

    // Create the depth buffer
    if (renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED) {
        ovrVkDepthBuffer_Create(
            context,
            &frameBuffer->Framebuffer.depthBuffer,
            renderPass->depthFormat,
            renderPass->sampleCount,
            frameBuffer->Width,
            frameBuffer->Height,
            isMultiview ? 2 : 1);
    }

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        uint32_t attachmentCount = 0;
        VkImageView attachments[4];

        if (renderPass->sampleCount > OVR_SAMPLE_COUNT_1) {
            attachments[attachmentCount++] = frameBuffer->Framebuffer.renderTexture.view;
        }
        if (renderPass->sampleCount <= OVR_SAMPLE_COUNT_1 || EXPLICIT_RESOLVE == 0) {
            attachments[attachmentCount++] = frameBuffer->Framebuffer.colorTextures[i].view;
        }
        if (renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED) {
            attachments[attachmentCount++] = frameBuffer->Framebuffer.depthBuffer.view;
        }
        if (frameBuffer->Framebuffer.fragmentDensityTextures != NULL &&
            renderPass->flags & OVR_RENDERPASS_FLAG_INCLUDE_FRAG_DENSITY) {
            attachments[attachmentCount++] =
                frameBuffer->Framebuffer.fragmentDensityTextures[i].view;
        }

        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = NULL;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = renderPass->renderPass;
        framebufferCreateInfo.attachmentCount = attachmentCount;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = frameBuffer->Framebuffer.width;
        framebufferCreateInfo.height = frameBuffer->Framebuffer.height;
        framebufferCreateInfo.layers = 1;

        VK(context->device->vkCreateFramebuffer(
            context->device->device,
            &framebufferCreateInfo,
            VK_ALLOCATOR,
            &frameBuffer->Framebuffer.framebuffers[i]));
    }

    return true;
}

static void ovrFramebuffer_Destroy(ovrVkContext* context, ovrFrameBuffer* frameBuffer) {
    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        if (frameBuffer->Framebuffer.framebuffers != NULL) {
            VC(context->device->vkDestroyFramebuffer(
                context->device->device, frameBuffer->Framebuffer.framebuffers[i], VK_ALLOCATOR));
        }
        if (frameBuffer->Framebuffer.colorTextures != NULL) {
            ovrVkTexture_Destroy(context, &frameBuffer->Framebuffer.colorTextures[i]);
        }
        if (frameBuffer->Framebuffer.fragmentDensityTextures != NULL) {
            ovrVkTexture_Destroy(context, &frameBuffer->Framebuffer.fragmentDensityTextures[i]);
        }
    }

    if (frameBuffer->Framebuffer.depthBuffer.image != VK_NULL_HANDLE) {
        ovrVkDepthBuffer_Destroy(context, &frameBuffer->Framebuffer.depthBuffer);
    }
    if (frameBuffer->Framebuffer.renderTexture.image != VK_NULL_HANDLE) {
        ovrVkTexture_Destroy(context, &frameBuffer->Framebuffer.renderTexture);
    }

    free(frameBuffer->Framebuffer.framebuffers);
    free(frameBuffer->Framebuffer.colorTextures);
    free(frameBuffer->Framebuffer.fragmentDensityTextures);

    vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

    ovrFramebuffer_Clear(frameBuffer);
}

/*
================================================================================

ovrScene

================================================================================
*/

#define NUM_INSTANCES 1500
#define NUM_ROTATIONS 16

typedef struct {
    bool CreatedScene;
    unsigned int Random;
    ovrVkGraphicsProgram Program;
    ovrVkGeometry Cube;
    ovrVkGraphicsPipeline Pipelines;
    ovrVkBuffer SceneMatrices;
    int NumViews;

    ovrVector3f Rotations[NUM_ROTATIONS];
    ovrVector3f CubePositions[NUM_INSTANCES];
    int CubeRotations[NUM_INSTANCES];
} ovrScene;

static void ovrScene_Clear(ovrScene* scene) {
    scene->CreatedScene = false;
    scene->Random = 2;
    memset(&scene->Program, 0, sizeof(ovrVkGraphicsProgram));
    memset(&scene->Cube, 0, sizeof(ovrVkGeometry));
    memset(&scene->Pipelines, 0, sizeof(ovrVkGraphicsPipeline));
    memset(&scene->SceneMatrices, 0, sizeof(ovrVkBuffer));
    scene->NumViews = 1;
}

static bool ovrScene_IsCreated(ovrScene* scene) {
    return scene->CreatedScene;
}

// Returns a random float in the range [0, 1].
static float ovrScene_RandomFloat(ovrScene* scene) {
    scene->Random = 1664525L * scene->Random + 1013904223L;
    unsigned int rf = 0x3F800000 | (scene->Random & 0x007FFFFF);
    return (*(float*)&rf) - 1.0f;
}

static void ovrScene_Create(ovrVkContext* context, ovrScene* scene, ovrVkRenderPass* renderPass) {
    const bool isMultiview = context->device->supportsMultiview;
    scene->NumViews = (isMultiview) ? 2 : 1;

    ovrGeometry_CreateCube(context, &scene->Cube);
    // Create the instance transform attribute buffer.
    ovrVkGeometry_AddInstanceAttributes(
        context, &scene->Cube, NUM_INSTANCES, OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM);

    ovrVkGraphicsProgram_Create(
        context,
        &scene->Program,
        isMultiview ? PROGRAM(colorOnlyMultiviewVertexProgram) : PROGRAM(colorOnlyVertexProgram),
        isMultiview ? sizeof(PROGRAM(colorOnlyMultiviewVertexProgram))
                    : sizeof(PROGRAM(colorOnlyVertexProgram)),
        PROGRAM(colorOnlyFragmentProgram),
        sizeof(PROGRAM(colorOnlyFragmentProgram)),
        colorOnlyProgramParms,
        ARRAY_SIZE(colorOnlyProgramParms),
        scene->Cube.layout,
        OVR_VERTEX_ATTRIBUTE_FLAG_POSITION | OVR_VERTEX_ATTRIBUTE_FLAG_COLOR |
            OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM);

    // Set up the graphics pipeline.
    ovrVkGraphicsPipelineParms pipelineParms;
    ovrVkGraphicsPipelineParms_Init(&pipelineParms);
    pipelineParms.renderPass = renderPass;
    pipelineParms.program = &scene->Program;
    pipelineParms.geometry = &scene->Cube;

    // ROP state.
    pipelineParms.rop.blendEnable = false;
    pipelineParms.rop.redWriteEnable = true;
    pipelineParms.rop.blueWriteEnable = true;
    pipelineParms.rop.greenWriteEnable = true;
    pipelineParms.rop.alphaWriteEnable = false;
    pipelineParms.rop.depthTestEnable = true;
    pipelineParms.rop.depthWriteEnable = true;
    pipelineParms.rop.frontFace = OVR_FRONT_FACE_CLOCKWISE;
    pipelineParms.rop.cullMode = OVR_CULL_MODE_BACK;
    pipelineParms.rop.depthCompare = OVR_COMPARE_OP_LESS_OR_EQUAL;
    pipelineParms.rop.blendColor.x = 0.0f;
    pipelineParms.rop.blendColor.y = 0.0f;
    pipelineParms.rop.blendColor.z = 0.0f;
    pipelineParms.rop.blendColor.w = 0.0f;
    pipelineParms.rop.blendOpColor = OVR_BLEND_OP_ADD;
    pipelineParms.rop.blendSrcColor = OVR_BLEND_FACTOR_ONE;
    pipelineParms.rop.blendDstColor = OVR_BLEND_FACTOR_ZERO;
    pipelineParms.rop.blendOpAlpha = OVR_BLEND_OP_ADD;
    pipelineParms.rop.blendSrcAlpha = OVR_BLEND_FACTOR_ONE;
    pipelineParms.rop.blendDstAlpha = OVR_BLEND_FACTOR_ZERO;

    ovrVkGraphicsPipeline_Create(context, &scene->Pipelines, &pipelineParms);

    // Setup the scene matrices.
    ovrVkBuffer_Create(
        context,
        &scene->SceneMatrices,
        OVR_BUFFER_TYPE_UNIFORM,
        2 * scene->NumViews * sizeof(ovrMatrix4f),
        NULL,
        false);

    // Setup random rotations.
    for (int i = 0; i < NUM_ROTATIONS; i++) {
        scene->Rotations[i].x = ovrScene_RandomFloat(scene);
        scene->Rotations[i].y = ovrScene_RandomFloat(scene);
        scene->Rotations[i].z = ovrScene_RandomFloat(scene);
    }

    // Setup random cube positions and rotations.
    for (int i = 0; i < NUM_INSTANCES; i++) {
        // Using volatile keeps the compiler from optimizing away multiple calls to
        // ovrScene_RandomFloat().
        volatile float rx, ry, rz;
        for (;;) {
            rx = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
            ry = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
            rz = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
            // If too close to 0,0,0
            if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f) {
                continue;
            }
            // Test for overlap with any of the existing cubes.
            bool overlap = false;
            for (int j = 0; j < i; j++) {
                if (fabsf(rx - scene->CubePositions[j].x) < 4.0f &&
                    fabsf(ry - scene->CubePositions[j].y) < 4.0f &&
                    fabsf(rz - scene->CubePositions[j].z) < 4.0f) {
                    overlap = true;
                    break;
                }
            }
            if (!overlap) {
                break;
            }
        }

        rx *= 0.1f;
        ry *= 0.1f;
        rz *= 0.1f;

        // Insert into list sorted based on distance.
        int insert = 0;
        const float distSqr = rx * rx + ry * ry + rz * rz;
        for (int j = i; j > 0; j--) {
            const ovrVector3f* otherPos = &scene->CubePositions[j - 1];
            const float otherDistSqr =
                otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
            if (distSqr > otherDistSqr) {
                insert = j;
                break;
            }
            scene->CubePositions[j] = scene->CubePositions[j - 1];
            scene->CubeRotations[j] = scene->CubeRotations[j - 1];
        }

        scene->CubePositions[insert].x = rx;
        scene->CubePositions[insert].y = ry;
        scene->CubePositions[insert].z = rz;

        scene->CubeRotations[insert] = (int)(ovrScene_RandomFloat(scene) * (NUM_ROTATIONS - 0.1f));
    }

    scene->CreatedScene = true;
}

static void ovrScene_Destroy(ovrVkContext* context, ovrScene* scene) {
    ovrVkContext_WaitIdle(context);

    ovrVkGraphicsPipeline_Destroy(context, &scene->Pipelines);

    ovrGeometry_Destroy(context, &scene->Cube);

    ovrVkGraphicsProgram_Destroy(context, &scene->Program);

    ovrVkBuffer_Destroy(context->device, &scene->SceneMatrices);

    scene->CreatedScene = false;
}

static void ovrScene_UpdateBuffers(
    ovrVkCommandBuffer* commandBuffer,
    ovrScene* scene,
    const ovrVector3f* currRotation,
    const ovrMatrix4f* viewMatrix,
    const ovrMatrix4f* projectionMatrix) {
    // Update the scene matrices uniform buffer.
    ovrMatrix4f* sceneMatrices = NULL;
    ovrVkBuffer* sceneMatricesBuffer =
        ovrVkCommandBuffer_MapBuffer(commandBuffer, &scene->SceneMatrices, (void**)&sceneMatrices);
    memcpy(sceneMatrices + 0 * scene->NumViews, viewMatrix, scene->NumViews * sizeof(ovrMatrix4f));
    memcpy(
        sceneMatrices + 1 * scene->NumViews,
        projectionMatrix,
        scene->NumViews * sizeof(ovrMatrix4f));
    ovrVkCommandBuffer_UnmapBuffer(
        commandBuffer, &scene->SceneMatrices, sceneMatricesBuffer, OVR_BUFFER_UNMAP_TYPE_COPY_BACK);

    ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
    for (int i = 0; i < NUM_ROTATIONS; i++) {
        rotationMatrices[i] = ovrMatrix4f_CreateRotation(
            scene->Rotations[i].x * currRotation->x,
            scene->Rotations[i].y * currRotation->y,
            scene->Rotations[i].z * currRotation->z);
    }

    // Update the instanced transform buffer.
    ovrDefaultVertexAttributeArrays attribs;
    ovrVkBuffer* instanceBuffer =
        ovrVkCommandBuffer_MapInstanceAttributes(commandBuffer, &scene->Cube, &attribs.base);

    ovrMatrix4f* transforms = &attribs.transform[0];
    for (int i = 0; i < NUM_INSTANCES; i++) {
        const int index = scene->CubeRotations[i];

        // Write in order in case the mapped buffer lives on write-combined memory.
        transforms[i].M[0][0] = rotationMatrices[index].M[0][0];
        transforms[i].M[0][1] = rotationMatrices[index].M[0][1];
        transforms[i].M[0][2] = rotationMatrices[index].M[0][2];
        transforms[i].M[0][3] = rotationMatrices[index].M[0][3];

        transforms[i].M[1][0] = rotationMatrices[index].M[1][0];
        transforms[i].M[1][1] = rotationMatrices[index].M[1][1];
        transforms[i].M[1][2] = rotationMatrices[index].M[1][2];
        transforms[i].M[1][3] = rotationMatrices[index].M[1][3];

        transforms[i].M[2][0] = rotationMatrices[index].M[2][0];
        transforms[i].M[2][1] = rotationMatrices[index].M[2][1];
        transforms[i].M[2][2] = rotationMatrices[index].M[2][2];
        transforms[i].M[2][3] = rotationMatrices[index].M[2][3];

        transforms[i].M[3][0] = scene->CubePositions[i].x;
        transforms[i].M[3][1] = scene->CubePositions[i].y;
        transforms[i].M[3][2] = scene->CubePositions[i].z;
        transforms[i].M[3][3] = 1.0f;
    }

    ovrVkCommandBuffer_UnmapInstanceAttributes(
        commandBuffer, &scene->Cube, instanceBuffer, OVR_BUFFER_UNMAP_TYPE_COPY_BACK);
}

static void ovrScene_Render(ovrVkCommandBuffer* commandBuffer, ovrScene* scene) {
    ovrVkGraphicsCommand command;
    ovrVkGraphicsCommand_Init(&command);
    ovrVkGraphicsCommand_SetPipeline(&command, &scene->Pipelines);
    ovrVkGraphicsCommand_SetParmBufferUniform(
        &command, PROGRAM_UNIFORM_SCENE_MATRICES, &scene->SceneMatrices);
    ovrVkGraphicsCommand_SetNumInstances(&command, NUM_INSTANCES);

    ovrVkCommandBuffer_SubmitGraphicsCommand(commandBuffer, &command);
}

/*
================================================================================

ovrSimulation

================================================================================
*/

typedef struct {
    ovrVector3f CurrentRotation;
} ovrSimulation;

static void ovrSimulation_Clear(ovrSimulation* simulation) {
    simulation->CurrentRotation.x = 0.0f;
    simulation->CurrentRotation.y = 0.0f;
    simulation->CurrentRotation.z = 0.0f;
}

static void ovrSimulation_Advance(ovrSimulation* simulation, double time) {
    // Update rotation.
    simulation->CurrentRotation.x = (float)(time);
    simulation->CurrentRotation.y = (float)(time);
    simulation->CurrentRotation.z = (float)(time);
}

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct {
    ovrVkRenderPass RenderPassSingleView;
    ovrVkCommandBuffer EyeCommandBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    ovrFrameBuffer Framebuffer[VRAPI_FRAME_LAYER_EYE_MAX];

    ovrMatrix4f ViewMatrix[VRAPI_FRAME_LAYER_EYE_MAX];
    ovrMatrix4f ProjectionMatrix[VRAPI_FRAME_LAYER_EYE_MAX];
    int NumEyes;
} ovrRenderer;

static void ovrRenderer_Clear(ovrRenderer* renderer) {
    memset(&renderer->RenderPassSingleView, 0, sizeof(ovrVkRenderPass));

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        memset(&renderer->EyeCommandBuffer[eye], 0, sizeof(ovrVkCommandBuffer));

        ovrFramebuffer_Clear(&renderer->Framebuffer[eye]);
    }

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        renderer->ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
        renderer->ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
    }

    renderer->NumEyes = VRAPI_FRAME_LAYER_EYE_MAX;
}

static void ovrRenderer_Create(ovrRenderer* renderer, ovrVkContext* context, const ovrJava* java) {
    bool isMultiview = context->device->supportsMultiview;
    bool useFFR = context->device->supportsFragmentDensity;
    renderer->NumEyes = isMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX;

    // Get swapchain images from vrapi first so that we know what attachments to use for the
    // renderpass
    ovrColorSwapChain colorSwapChains[renderer->NumEyes];
    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        ovrColorSwapChain_Init(
            &colorSwapChains[eye],
            VK_FORMAT_R8G8B8A8_UNORM,
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
            isMultiview);
        useFFR = useFFR && colorSwapChains[eye].FragmentDensityTextures != NULL;
    }

    ovrVector4f clearColor = {0.125f, 0.0f, 0.125f, 1.0f};
    int flags = OVR_RENDERPASS_FLAG_CLEAR_COLOR_BUFFER | OVR_RENDERPASS_FLAG_CLEAR_DEPTH_BUFFER;
    if (useFFR) {
        flags |= OVR_RENDERPASS_FLAG_INCLUDE_FRAG_DENSITY;
    }
    ovrVkRenderPass_Create(
        context,
        &renderer->RenderPassSingleView,
        OVR_SURFACE_COLOR_FORMAT_R8G8B8A8,
        OVR_SURFACE_DEPTH_FORMAT_D24,
        SAMPLE_COUNT,
        OVR_RENDERPASS_TYPE_INLINE,
        flags,
        &clearColor,
        isMultiview);

    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        ovrFramebuffer_Create(
            context,
            &renderer->Framebuffer[eye],
            &renderer->RenderPassSingleView,
            &colorSwapChains[eye],
            VK_FORMAT_R8G8B8A8_UNORM,
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
            isMultiview);
        ovrColorSwapChain_Destroy(&colorSwapChains[eye]);

        ovrVkCommandBuffer_Create(
            context,
            &renderer->EyeCommandBuffer[eye],
            OVR_COMMAND_BUFFER_TYPE_PRIMARY,
            ovrVkFramebuffer_GetBufferCount(&renderer->Framebuffer[eye].Framebuffer));
    }

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        renderer->ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
        renderer->ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
    }
}

static void ovrRenderer_Destroy(ovrRenderer* renderer, ovrVkContext* context) {
    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        ovrFramebuffer_Destroy(context, &renderer->Framebuffer[eye]);

        ovrVkCommandBuffer_Destroy(context, &renderer->EyeCommandBuffer[eye]);
    }

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        renderer->ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
        renderer->ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
    }

    ovrVkRenderPass_Destroy(context, &renderer->RenderPassSingleView);
}

static ovrLayerProjection2 ovrRenderer_RenderFrame(
    ovrRenderer* renderer,
    long long frameIndex,
    ovrScene* scene,
    const ovrSimulation* simulation,
    const ovrTracking2* tracking) {
    ovrMatrix4f eyeViewMatrixTransposed[2];
    eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&tracking->Eye[0].ViewMatrix);
    eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&tracking->Eye[1].ViewMatrix);

    renderer->ViewMatrix[0] = eyeViewMatrixTransposed[0];
    renderer->ViewMatrix[1] = eyeViewMatrixTransposed[1];

    ovrMatrix4f projectionMatrixTransposed[2];
    projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&tracking->Eye[0].ProjectionMatrix);
    projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&tracking->Eye[1].ProjectionMatrix);

    renderer->ProjectionMatrix[0] = projectionMatrixTransposed[0];
    renderer->ProjectionMatrix[1] = projectionMatrixTransposed[1];

    // Render the scene.

    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        const ovrScreenRect screenRect =
            ovrVkFramebuffer_GetRect(&renderer->Framebuffer[eye].Framebuffer);

        ovrVkCommandBuffer_BeginPrimary(&renderer->EyeCommandBuffer[eye]);
        ovrVkCommandBuffer_BeginFramebuffer(
            &renderer->EyeCommandBuffer[eye],
            &renderer->Framebuffer[eye].Framebuffer,
            0 /*eye*/,
            OVR_TEXTURE_USAGE_COLOR_ATTACHMENT);

        // Update the instance transform attributes.
        // NOTE: needs to be called before ovrVkCommandBuffer_BeginRenderPass when current render
        // pass is not set.
        ovrScene_UpdateBuffers(
            &renderer->EyeCommandBuffer[eye],
            scene,
            &simulation->CurrentRotation,
            &renderer->ViewMatrix[eye],
            &renderer->ProjectionMatrix[eye]);

        ovrVkCommandBuffer_BeginRenderPass(
            &renderer->EyeCommandBuffer[eye],
            &renderer->RenderPassSingleView,
            &renderer->Framebuffer[eye].Framebuffer,
            &screenRect);

        ovrVkCommandBuffer_SetViewport(&renderer->EyeCommandBuffer[eye], &screenRect);
        ovrVkCommandBuffer_SetScissor(&renderer->EyeCommandBuffer[eye], &screenRect);

        ovrScene_Render(&renderer->EyeCommandBuffer[eye], scene);

        ovrVkCommandBuffer_EndRenderPass(
            &renderer->EyeCommandBuffer[eye], &renderer->RenderPassSingleView);

        ovrVkCommandBuffer_EndFramebuffer(
            &renderer->EyeCommandBuffer[eye],
            &renderer->Framebuffer[eye].Framebuffer,
            0 /*eye*/,
            OVR_TEXTURE_USAGE_SAMPLED);
        ovrVkCommandBuffer_EndPrimary(&renderer->EyeCommandBuffer[eye]);

        ovrVkCommandBuffer_SubmitPrimary(&renderer->EyeCommandBuffer[eye]);
    }

    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = tracking->HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        int eyeToSample = renderer->NumEyes == 1 ? 0 : eye;
        layer.Textures[eye].ColorSwapChain =
            renderer->Framebuffer[eyeToSample].ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex =
            renderer->Framebuffer[eyeToSample].Framebuffer.currentBuffer;
        layer.Textures[eye].TexCoordsFromTanAngles =
            ovrMatrix4f_TanAngleMatrixFromProjection(&tracking->Eye[eye].ProjectionMatrix);
    }

    return layer;
}

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct {
    ovrJava Java;
    ANativeWindow* NativeWindow;
    bool Resumed;
    ovrVkDevice Device;
    ovrVkContext Context;
    ovrMobile* Ovr;
    ovrScene Scene;
    ovrSimulation Simulation;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    int MainThreadTid;
    int RenderThreadTid;
    ovrRenderer Renderer;
} ovrApp;

static void ovrApp_Clear(ovrApp* app) {
    app->Java.Vm = NULL;
    app->Java.Env = NULL;
    app->Java.ActivityObject = NULL;
    app->NativeWindow = NULL;
    app->Resumed = false;
    memset(&app->Device, 0, sizeof(ovrVkDevice));
    memset(&app->Context, 0, sizeof(ovrVkContext));
    app->Ovr = NULL;
    app->FrameIndex = 1;
    app->DisplayTime = 0;
    app->SwapInterval = 1;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;

    ovrScene_Clear(&app->Scene);
    ovrSimulation_Clear(&app->Simulation);
    ovrRenderer_Clear(&app->Renderer);
}

static void ovrApp_HandleVrModeChanges(ovrApp* app) {
    if (app->Resumed != false && app->NativeWindow != NULL) {
        if (app->Ovr == NULL) {
            ovrModeParmsVulkan parms =
                vrapi_DefaultModeParmsVulkan(&app->Java, (unsigned long long)app->Context.queue);
            // No need to reset the FLAG_FULLSCREEN window flag when using a View
            parms.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

            parms.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            parms.ModeParms.WindowSurface = (size_t)app->NativeWindow;
            // Leave explicit egl objects defaulted.
            parms.ModeParms.Display = 0;
            parms.ModeParms.ShareContext = 0;

            ALOGV("        vrapi_EnterVrMode()");

            app->Ovr = vrapi_EnterVrMode((ovrModeParms*)&parms);

            // If entering VR mode failed then the ANativeWindow was not valid.
            if (app->Ovr == NULL) {
                ALOGE("Invalid ANativeWindow!");
                app->NativeWindow = NULL;
            }

            // Set performance parameters once we have entered VR mode and have a valid ovrMobile.
            if (app->Ovr != NULL) {
                vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);

                ALOGV("		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel);

                vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);

                ALOGV("		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid);

                vrapi_SetPerfThread(
                    app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);

                ALOGV("		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid);
            }
        }
    } else {
        if (app->Ovr != NULL) {
            ALOGV("        vrapi_LeaveVrMode()");

            vrapi_LeaveVrMode(app->Ovr);
            app->Ovr = NULL;
        }
    }
}

static void ovrApp_HandleInput(ovrApp* app) {}

static void ovrApp_HandleVrApiEvents(ovrApp* app) {
    ovrEventDataBuffer eventDataBuffer = {};

    // Poll for VrApi events
    for (;;) {
        ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
        ovrResult res = vrapi_PollEvent(eventHeader);
        if (res != ovrSuccess) {
            break;
        }

        switch (eventHeader->EventType) {
            case VRAPI_EVENT_DATA_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
                break;
            case VRAPI_EVENT_VISIBILITY_GAINED:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
                break;
            case VRAPI_EVENT_VISIBILITY_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
                break;
            case VRAPI_EVENT_FOCUS_GAINED:
                // FOCUS_GAINED is sent when the application is in the foreground and has
                // input focus. This may be due to a system overlay relinquishing focus
                // back to the application.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
                break;
            case VRAPI_EVENT_FOCUS_LOST:
                // FOCUS_LOST is sent when the application is no longer in the foreground and
                // therefore does not have input focus. This may be due to a system overlay taking
                // focus from the application. The application should take appropriate action when
                // this occurs.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
                break;
            default:
                ALOGV("vrapi_PollEvent: Unknown event");
                break;
        }
    }
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    ovrApp* appState = (ovrApp*)app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    ovrJava java;
    java.Vm = app->activity->vm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = app->activity->clazz;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

    ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    initParms.GraphicsAPI = VRAPI_GRAPHICS_API_VULKAN_1;
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        ALOGE("Failed to initialize VrApi");
        // If intialization failed, vrapi_* function calls will not be available.
        exit(0);
    }

    ovrApp appState;
    ovrApp_Clear(&appState);
    appState.Java = java;

    char instanceExtensionNames[4096];
    uint32_t instanceExtensionNamesSize = sizeof(instanceExtensionNames);

    // Get the required instance extensions.
    if (vrapi_GetInstanceExtensionsVulkan(instanceExtensionNames, &instanceExtensionNamesSize)) {
        ALOGE("vrapi_GetInstanceExtensionsVulkan FAILED");
        vrapi_Shutdown();
        exit(0);
    }

    // Create the Vulkan instance.
    ovrVkInstance instance;
    if (ovrVkInstance_Create(&instance, instanceExtensionNames, instanceExtensionNamesSize) ==
        false) {
        ALOGE("Failed to create vulkan instance");
        vrapi_Shutdown();
        exit(0);
    }

    // Get the required device extensions.
    char deviceExtensionNames[4096];
    uint32_t deviceExtensionNamesSize = sizeof(deviceExtensionNames);

    if (vrapi_GetDeviceExtensionsVulkan(deviceExtensionNames, &deviceExtensionNamesSize)) {
        ALOGE("vrapi_GetDeviceExtensionsVulkan FAILED");
        vrapi_Shutdown();
        exit(0);
    }

    // Select the physical device.
    if (ovrVkDevice_SelectPhysicalDevice(
            &appState.Device, &instance, deviceExtensionNames, deviceExtensionNamesSize) == false) {
        ALOGE("Failed to select physical device");
        vrapi_Shutdown();
        exit(0);
    }

    // Create the Vulkan device
    if (ovrVkDevice_Create(&appState.Device, &instance) == false) {
        ALOGE("Failed to create vulkan device");
        vrapi_Shutdown();
        exit(0);
    }

    // Set up the vulkan queue and command buffer
    if (ovrVkContext_Create(&appState.Context, &appState.Device, 0 /* queueIndex */) == false) {
        ALOGE("Failed to create a valid vulkan context");
        vrapi_Shutdown();
        exit(0);
    }

    ovrSystemCreateInfoVulkan systemInfo;
    systemInfo.Instance = instance.instance;
    systemInfo.Device = appState.Device.device;
    systemInfo.PhysicalDevice = appState.Device.physicalDevice;
    initResult = vrapi_CreateSystemVulkan(&systemInfo);
    if (initResult != ovrSuccess) {
        ALOGE("Failed to create VrApi Vulkan System");
        vrapi_Shutdown();
        exit(0);
    }

    appState.CpuLevel = CPU_LEVEL;
    appState.GpuLevel = GPU_LEVEL;
    appState.MainThreadTid = gettid();

    ovrRenderer_Create(&appState.Renderer, &appState.Context, &appState.Java);

    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    while (app->destroyRequested == 0) {
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source* source;
            const int timeoutMilliseconds =
                (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(app, source);
            }

            ovrApp_HandleVrModeChanges(&appState);
        }

        // We must read from the event queue with regular frequency.
        ovrApp_HandleVrApiEvents(&appState);

        ovrApp_HandleInput(&appState);

        if (appState.Ovr == NULL) {
            continue;
        }

        // Create the scene if not yet created.
        // The scene is created here to be able to show a loading icon.
        if (!ovrScene_IsCreated(&appState.Scene)) {
            // Show a loading icon.
            int frameFlags = 0;
            frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

            ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
            blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
            iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            const ovrLayerHeader2* layers[] = {
                &blackLayer.Header,
                &iconLayer.Header,
            };

            ovrSubmitFrameDescription2 frameDesc = {0};
            frameDesc.Flags = frameFlags;
            frameDesc.SwapInterval = 1;
            frameDesc.FrameIndex = appState.FrameIndex;
            frameDesc.DisplayTime = appState.DisplayTime;
            frameDesc.LayerCount = 2;
            frameDesc.Layers = layers;

            vrapi_SubmitFrame2(appState.Ovr, &frameDesc);

            // Create the scene.
            ovrScene_Create(
                &appState.Context, &appState.Scene, &appState.Renderer.RenderPassSingleView);
        }

        //----------------------
        // Simulation
        //----------------------

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Get the HMD pose, predicted for the middle of the time period during which
        // the new eye images will be displayed. The number of frames predicted ahead
        // depends on the pipeline depth of the engine and the synthesis rate.
        // The better the prediction, the less black will be pulled in at the edges.
        const double predictedDisplayTime =
            vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
        const ovrTracking2 tracking =
            vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);

        appState.DisplayTime = predictedDisplayTime;

        // Advance the simulation based on the predicted display time.
        ovrSimulation_Advance(&appState.Simulation, predictedDisplayTime);

        // Render eye images and setup ovrFrameParms using ovrTracking2.
        const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame(
            &appState.Renderer,
            appState.FrameIndex,
            &appState.Scene,
            &appState.Simulation,
            &tracking);

        const ovrLayerHeader2* layers[] = {&worldLayer.Header};

        ovrSubmitFrameDescription2 frameDesc = {0};
        frameDesc.Flags = 0;
        frameDesc.SwapInterval = appState.SwapInterval;
        frameDesc.FrameIndex = appState.FrameIndex;
        frameDesc.DisplayTime = appState.DisplayTime;
        frameDesc.LayerCount = 1;
        frameDesc.Layers = layers;

        // Hand over the eye images to the time warp.
        vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
    }

    ovrRenderer_Destroy(&appState.Renderer, &appState.Context);

    ovrScene_Destroy(&appState.Context, &appState.Scene);

    vrapi_DestroySystemVulkan();

    ovrVkContext_Destroy(&appState.Context);
    ovrVkDevice_Destroy(&appState.Device);
    ovrVkInstance_Destroy(&instance);

    vrapi_Shutdown();

    (*java.Vm)->DetachCurrentThread(java.Vm);
}
