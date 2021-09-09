/************************************************************************************

Filename    :   DebugLines.cpp
Content     :   Class that manages and renders debug lines.
Created     :   April 22, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "DebugLines.h"

#include <stdlib.h>

#include "Egl.h"
#include "Misc/Log.h"

#include "GlGeometry.h"
#include "GlProgram.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const char* DebugLineVertexSrc = R"glsl(
	attribute vec4 Position;
	attribute vec4 VertexColor;
	varying lowp vec4 outColor;
	void main()
	{
	   gl_Position = TransformVertex( Position );
	   outColor = VertexColor;
	}
)glsl";

static const char* DebugLineFragmentSrc = R"glsl(
	varying lowp vec4 outColor;
	void main()
	{
		gl_FragColor = outColor;
	}
)glsl";

//==============================================================
// OvrDebugLinesLocal
//
class OvrDebugLinesLocal : public OvrDebugLines {
   public:
    struct DebugLines_t {
        DebugLines_t() : DrawSurf(&Surf) {}

        ovrSurfaceDef Surf;
        ovrDrawSurface DrawSurf;
        VertexAttribs Attr;
        std::vector<long long> EndFrame;
    };

    typedef unsigned short LineIndex_t;

    static const int MAX_DEBUG_LINES = 2048;

    OvrDebugLinesLocal();
    virtual ~OvrDebugLinesLocal();

    virtual void Init();
    virtual void Shutdown();

    virtual void BeginFrame(const long long frameNum);
    virtual void AppendSurfaceList(std::vector<ovrDrawSurface>& surfaceList);

    virtual void AddLine(
        const Vector3f& start,
        const Vector3f& end,
        const Vector4f& startColor,
        const Vector4f& endColor,
        const long long endFrame,
        const bool depthTest);
    virtual void AddPoint(
        const Vector3f& pos,
        const float size,
        const Vector4f& color,
        const long long endFrame,
        const bool depthTest);
    // Add a debug point without a specified color. The axis lines will use default
    // colors: X = red, Y = green, Z = blue (same as Maya).
    virtual void
    AddPoint(const Vector3f& pos, const float size, const long long endFrame, const bool depthTest);

    virtual void AddBounds(Posef const& pose, Bounds3f const& bounds, Vector4f const& color);

    virtual void AddAxes(
        const Vector3f& origin,
        const Matrix4f& axes,
        const float size,
        const Vector4f& color,
        const long long endFrame,
        const bool depthTest);

   private:
    DebugLines_t DepthTested;
    DebugLines_t NonDepthTested;

    bool Initialized;
    GlProgram LineProgram;

    void RemoveExpired(const long long frameNum, DebugLines_t& lines);
};

//==============================
// OvrDebugLinesLocal::OvrDebugLinesLocal
OvrDebugLinesLocal::OvrDebugLinesLocal() : Initialized(false) {}

//==============================
// OvrDebugLinesLocal::OvrDebugLinesLocal
OvrDebugLinesLocal::~OvrDebugLinesLocal() {
    Shutdown();
}

//==============================
// OvrDebugLinesLocal::Init
void OvrDebugLinesLocal::Init() {
    if (Initialized) {
        // JDC: multi-activity test		ASSERT_WITH_TAG( !Initialized, "DebugLines" );
        return;
    }

    // this is only freed by the OS when the program exits
    if (LineProgram.VertexShader == 0 || LineProgram.FragmentShader == 0) {
        LineProgram = GlProgram::Build(DebugLineVertexSrc, DebugLineFragmentSrc, NULL, 0);
    }

    // the indices will never change once we've set them up, we just won't necessarily
    // use all of the index buffer to render.
    const int MAX_INDICES = MAX_DEBUG_LINES * 2;
    std::vector<LineIndex_t> indices;
    indices.reserve(MAX_INDICES);

    for (LineIndex_t i = 0; i < MAX_INDICES; ++i) {
        indices.push_back(i);
    }

    for (int i = 0; i < 2; i++) {
        DebugLines_t& dl = i == 0 ? NonDepthTested : DepthTested;
        dl.Surf.geo.Create(dl.Attr, indices);
        dl.Surf.geo.primitiveType = GL_LINES;
        ovrGraphicsCommand& gc = dl.Surf.graphicsCommand;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = i == 1;
        gc.GpuState.lineWidth = 2.0f;
        gc.Program = LineProgram;
    }

    Initialized = true;
}

//==============================
// OvrDebugLinesLocal::Shutdown
void OvrDebugLinesLocal::Shutdown() {
    if (!Initialized) {
        // OVR_ASSERT_WITH_TAG( !Initialized, "DebugLines" );
        return;
    }
    DepthTested.Surf.geo.Free();
    NonDepthTested.Surf.geo.Free();
    GlProgram::Free(LineProgram);
    Initialized = false;
}

//==============================
// OvrDebugLinesLocal::AppendSurfaceList
void OvrDebugLinesLocal::AppendSurfaceList(std::vector<ovrDrawSurface>& surfaceList) {
    for (int j = 0; j < 2; j++) {
        DebugLines_t& dl = j == 0 ? NonDepthTested : DepthTested;
        int verts = dl.Attr.position.size();
        if (verts == 0) {
            continue;
        }
        dl.Surf.geo.Update(dl.Attr);
        dl.Surf.geo.indexCount = verts;
        surfaceList.push_back(dl.DrawSurf);
    }
}

//==============================
// OvrDebugLinesLocal::AddLine
void OvrDebugLinesLocal::AddLine(
    const Vector3f& start,
    const Vector3f& end,
    const Vector4f& startColor,
    const Vector4f& endColor,
    const long long endFrame,
    const bool depthTest) {
    // OVR_LOG( "OvrDebugLinesLocal::AddDebugLine" );
    DebugLines_t& dl = depthTest ? DepthTested : NonDepthTested;
    dl.Attr.position.push_back(start);
    dl.Attr.position.push_back(end);
    dl.Attr.color.push_back(startColor);
    dl.Attr.color.push_back(endColor);
    dl.EndFrame.push_back(endFrame);
    // OVR_ASSERT( DepthTested.EndFrame.GetSizeI() < MAX_DEBUG_LINES );
    // OVR_ASSERT( NonDepthTested.EndFrame.GetSizeI() < MAX_DEBUG_LINES );
}

//==============================
// OvrDebugLinesLocal::AddPoint
void OvrDebugLinesLocal::AddPoint(
    const Vector3f& pos,
    const float size,
    const Vector4f& color,
    const long long endFrame,
    const bool depthTest) {
    float const hs = size * 0.5f;
    Vector3f const fwd(0.0f, 0.0f, hs);
    Vector3f const right(hs, 0.0f, 0.0f);
    Vector3f const up(0.0f, hs, 0.0f);

    AddLine(pos - fwd, pos + fwd, color, color, endFrame, depthTest);
    AddLine(pos - right, pos + right, color, color, endFrame, depthTest);
    AddLine(pos - up, pos + up, color, color, endFrame, depthTest);
}

//==============================
// OvrDebugLinesLocal::AddPoint
void OvrDebugLinesLocal::AddPoint(
    const Vector3f& pos,
    const float size,
    const long long endFrame,
    const bool depthTest) {
    float const hs = size * 0.5f;
    Vector3f const fwd(0.0f, 0.0f, hs);
    Vector3f const right(hs, 0.0f, 0.0f);
    Vector3f const up(0.0f, hs, 0.0f);

    AddLine(
        pos - fwd,
        pos + fwd,
        Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
        Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
        endFrame,
        depthTest);
    AddLine(
        pos - right,
        pos + right,
        Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
        Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
        endFrame,
        depthTest);
    AddLine(
        pos - up,
        pos + up,
        Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
        Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
        endFrame,
        depthTest);
}

//==============================
// OvrDebugLinesLocal::AddAxes
void OvrDebugLinesLocal::AddAxes(
    const Vector3f& origin,
    const Matrix4f& axes,
    const float size,
    const Vector4f& color,
    const long long endFrame,
    const bool depthTest) {
    const float half_size = size * 0.5f;
    Vector3f const fwd = Vector3f(axes.M[2][0], axes.M[2][1], axes.M[2][2]) * half_size;
    Vector3f const right = Vector3f(axes.M[0][0], axes.M[0][1], axes.M[0][2]) * half_size;
    Vector3f const up = Vector3f(axes.M[1][0], axes.M[1][1], axes.M[1][2]) * half_size;

    AddLine(
        origin - fwd,
        origin + fwd,
        Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
        Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
        endFrame,
        depthTest);
    AddLine(
        origin - right,
        origin + right,
        Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
        Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
        endFrame,
        depthTest);
    AddLine(
        origin - up,
        origin + up,
        Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
        Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
        endFrame,
        depthTest);
}

//==============================
// OvrDebugLinesLocal::AddBounds
void OvrDebugLinesLocal::AddBounds(
    Posef const& pose,
    Bounds3f const& bounds,
    Vector4f const& color) {
    Vector3f const& mins = bounds.GetMins();
    Vector3f const& maxs = bounds.GetMaxs();
    Vector3f corners[8];
    corners[0] = mins;
    corners[7] = maxs;
    corners[1] = Vector3f(mins.x, maxs.y, mins.z);
    corners[2] = Vector3f(mins.x, maxs.y, maxs.z);
    corners[3] = Vector3f(mins.x, mins.y, maxs.z);
    corners[4] = Vector3f(maxs.x, mins.y, mins.z);
    corners[5] = Vector3f(maxs.x, maxs.y, mins.z);
    corners[6] = Vector3f(maxs.x, mins.y, maxs.z);

    // transform points
    for (int i = 0; i < 8; ++i) {
        corners[i] = pose.Rotation.Rotate(corners[i]);
        corners[i] += pose.Translation;
    }

    AddLine(corners[0], corners[1], color, color, 1, true);
    AddLine(corners[1], corners[2], color, color, 1, true);
    AddLine(corners[2], corners[3], color, color, 1, true);
    AddLine(corners[3], corners[0], color, color, 1, true);
    AddLine(corners[7], corners[6], color, color, 1, true);
    AddLine(corners[6], corners[4], color, color, 1, true);
    AddLine(corners[4], corners[5], color, color, 1, true);
    AddLine(corners[5], corners[7], color, color, 1, true);
    AddLine(corners[0], corners[4], color, color, 1, true);
    AddLine(corners[1], corners[5], color, color, 1, true);
    AddLine(corners[2], corners[7], color, color, 1, true);
    AddLine(corners[3], corners[6], color, color, 1, true);
}

//==============================
// OvrDebugLinesLocal::BeginFrame
void OvrDebugLinesLocal::BeginFrame(const long long frameNum) {
    // LOG( "OvrDebugLinesLocal::RemoveExpired: frame %lli, removing %i lines", frameNum,
    // DepthTestedLines.GetSizeI() + NonDepthTestedLines.GetSizeI() );
    DepthTested.Surf.geo.indexCount = 0;
    NonDepthTested.Surf.geo.indexCount = 0;
    RemoveExpired(frameNum, DepthTested);
    RemoveExpired(frameNum, NonDepthTested);
}

//==============================
// OvrDebugLinesLocal::RemoveExpired
void OvrDebugLinesLocal::RemoveExpired(const long long frameNum, DebugLines_t& lines) {
    for (int i = lines.EndFrame.size() - 1; i >= 0; --i) {
        if (frameNum >= lines.EndFrame[i]) {
            lines.Attr.position.erase(
                lines.Attr.position.cbegin() + (i * 2), lines.Attr.position.cbegin() + (i * 2) + 2);
            lines.Attr.color.erase(
                lines.Attr.color.cbegin() + (i * 2), lines.Attr.color.cbegin() + (i * 2) + 2);
            lines.EndFrame.erase(lines.EndFrame.cbegin() + i);
        }
    }
}

//==============================
// OvrDebugLines::Create
OvrDebugLines* OvrDebugLines::Create() {
    return new OvrDebugLinesLocal;
}

//==============================
// OvrDebugLines::Free
void OvrDebugLines::Free(OvrDebugLines*& debugLines) {
    if (debugLines != NULL) {
        delete debugLines;
        debugLines = NULL;
    }
}

} // namespace OVRFW
