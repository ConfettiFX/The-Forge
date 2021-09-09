/************************************************************************************

Filename    :   PanelRenderer.cpp
Content     :   Class that manages and renders quad-based panels with custom shaders.
Created     :   September 19, 2019
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#include "PanelRenderer.h"

#include "Misc/Log.h"
#include "GlGeometry.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const char* VertexSrc = R"glsl(
attribute vec4 Position;
attribute vec2 TexCoord;

varying highp vec2 oTexCoord;

void main()
{
	gl_Position = TransformVertex( Position );
	oTexCoord = TexCoord;
}
)glsl";

static const char* FragmentSrc = R"glsl(
uniform highp vec4 ChannelEnable;
uniform highp vec2 GraphOffset;
uniform highp vec4 ChannelColor0;
uniform highp vec4 ChannelColor1;
uniform highp vec4 ChannelColor2;
uniform highp vec4 ChannelColor3;
uniform ChannelData
{
	highp vec4 dataSample[256];
} cd;

varying highp vec2 oTexCoord;

void main()
{
	vec2 pixelPos = vec2( (oTexCoord.x + GraphOffset.x) * 256.0, oTexCoord.y * 256.0);
	vec4 dataS = cd.dataSample[ int(pixelPos.x) & 0x00FF ];

	vec4 noData = vec4(0);
	float invCoord = 1.0 - oTexCoord.y;

	// first channel 
	vec4 color0 = dataS.x > invCoord ? ChannelColor0 : noData;
	vec4 color1 = dataS.y > invCoord ? ChannelColor1 : noData;
	vec4 color2 = dataS.z > invCoord ? ChannelColor2 : noData;
	vec4 color4 = dataS.w > invCoord ? ChannelColor3 : noData;

	vec4 aggregate = 
		color0 * ChannelEnable.x +
		color1 * ChannelEnable.y +
		color2 * ChannelEnable.z +
		color4 * ChannelEnable.w;

	gl_FragColor = min( aggregate, vec4(1) );
}
)glsl";

static ovrProgramParm UniformParms[] = {
    {"ChannelEnable", ovrProgramParmType::FLOAT_VECTOR4},
    {"GraphOffset", ovrProgramParmType::FLOAT_VECTOR2},
    {"ChannelData", ovrProgramParmType::BUFFER_UNIFORM},
    {"ChannelColor0", ovrProgramParmType::FLOAT_VECTOR4},
    {"ChannelColor1", ovrProgramParmType::FLOAT_VECTOR4},
    {"ChannelColor2", ovrProgramParmType::FLOAT_VECTOR4},
    {"ChannelColor3", ovrProgramParmType::FLOAT_VECTOR4},
};

void ovrPanelRenderer::Init() {
    Program = GlProgram::Build(
        VertexSrc, FragmentSrc, UniformParms, sizeof(UniformParms) / sizeof(UniformParms[0]));

    SurfaceDef.geo = BuildTesselatedQuad(1, 1, false);

    ChannelDataBuffer.Create(
        GLBUFFER_TYPE_UNIFORM, NUM_DATA_POINTS * sizeof(Vector4f), UniformChannelData.data());

    /// Hook the graphics command
    ovrGraphicsCommand& gc = SurfaceDef.graphicsCommand;
    gc.Program = Program;
    gc.UniformData[0].Data = &UniformChannelEnable;
    gc.UniformData[1].Data = &UniformGraphOffset;
    gc.UniformData[2].Count = NUM_DATA_POINTS;
    gc.UniformData[2].Data = &ChannelDataBuffer;
    gc.UniformData[3].Data = &UniformChannelColor[0];
    gc.UniformData[4].Data = &UniformChannelColor[1];
    gc.UniformData[5].Data = &UniformChannelColor[2];
    gc.UniformData[6].Data = &UniformChannelColor[3];

    /// gpu state needs alpha blending
    gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
    gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    gc.GpuState.blendSrc = GL_SRC_ALPHA;
    gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
}

void ovrPanelRenderer::Shutdown() {}

void ovrPanelRenderer::Update(const OVR::Vector4f& dataPoint) {
    // Update data
    UniformChannelData[WritePosition] = dataPoint;

    // Update rendering offset
    const float dataOffsetSize = static_cast<float>(NUM_DATA_POINTS - 1);
    UniformGraphOffset.x = static_cast<float>(WritePosition) / dataOffsetSize;

    // Move circular buffer
    WritePosition = (WritePosition + 1) % NUM_DATA_POINTS;

    /// Update the shader uniform parameters
    ChannelDataBuffer.Update(
        UniformChannelData.size() * sizeof(Vector4f), UniformChannelData.data());
}

void ovrPanelRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    if (SurfaceDef.geo.indexCount > 0) {
        surfaceList.push_back(ovrDrawSurface(ModelMatrix, &SurfaceDef));
    }
}

} // namespace OVRFW
