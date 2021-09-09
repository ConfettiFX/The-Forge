/************************************************************************************

Filename    :   GazeCursor.cpp
Content     :   Global gaze cursor.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "GazeCursor.h"

#include "Misc/Log.h"
#include "Render/GlTexture.h"
#include "Render/GlProgram.h"
#include "Render/GlGeometry.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

inline Vector3f GetViewMatrixPosition(Matrix4f const& m) {
    return m.Inverted().GetTranslation();
}

inline Vector3f GetViewMatrixForward(Matrix4f const& m) {
    return Vector3f(-m.M[2][0], -m.M[2][1], -m.M[2][2]).Normalized();
}

inline Vector3f GetViewMatrixUp(Matrix4f const& m) {
    return Vector3f(-m.M[1][0], -m.M[1][1], -m.M[1][2]).Normalized();
}

namespace OVRFW {

static const char* GazeCursorVertexSrc = R"glsl(
	uniform vec4 UniformColor;
	attribute vec4 Position;
	attribute vec2 TexCoord;
	attribute vec4 VertexColor;
	varying  highp vec2 oTexCoord;
	varying  lowp vec4 oColor;
	void main()
	{
	   gl_Position = TransformVertex( Position );
	   oTexCoord = TexCoord;
		oColor = VertexColor * UniformColor;
	}
)glsl";

static const char* GazeCursorFragmentSrc = R"glsl(
	uniform sampler2D Texture0;
	varying highp vec2 oTexCoord;
	varying lowp vec4 oColor;
	void main()
	{
		gl_FragColor = oColor * texture2D( Texture0, oTexCoord );
	}
)glsl";

//==============================================================
// OvrGazeCursorLocal
//
// Private implementation of the gaze cursor interface.
class OvrGazeCursorLocal : public OvrGazeCursor {
   public:
    static const float CURSOR_MAX_DIST;
    static const int TRAIL_GHOSTS = 1;

    OvrGazeCursorLocal();
    virtual ~OvrGazeCursorLocal();

    // Initialize the gaze cursor system.
    virtual void Init(ovrFileSys& fileSys);

    // Shutdown the gaze cursor system.
    virtual void Shutdown();

    // Updates the gaze cursor distance if ths distance passed is less than the current
    // distance.  System that use the gaze cursor should use this method so that they
    // interact civilly with other systems using the gaze cursor.
    virtual void UpdateDistance(float const d, eGazeCursorStateType const state);

    // Force the distance to a specific value -- this will set the distance even if
    // it is further away than the current distance. Unless your intent is to overload
    // the distance set by all other systems that use the gaze cursor, don't use this.
    virtual void ForceDistance(float const d, eGazeCursorStateType const state);

    // Call when the scene changes or the camera moves a large amount to clear out the cursor trail
    virtual void ClearGhosts();

    // Called once per frame to update logic.
    virtual void
    Frame(Matrix4f const& viewMatrix, Matrix4f const& traceMatrix, float const deltaTime);

    // Generates the gaze cursor surfaces and appends to the surface render list.
    virtual void AppendSurfaceList(std::vector<ovrDrawSurface>& surfaceList) const;

    // Returns the current info about the gaze cursor.
    virtual OvrGazeCursorInfo GetInfo() const;

    // Sets the rate at which the gaze cursor icon will spin.
    virtual void SetRotationRate(float const degreesPerSec);

    // Sets the scale factor for the cursor's size.
    virtual void SetCursorScale(float const scale);

    // Returns whether the gaze cursor will be drawn this frame
    virtual bool IsVisible() const;

    // Hide the gaze cursor.
    virtual void HideCursor() {
        Hidden = true;
    }

    // Show the gaze cursor.
    virtual void ShowCursor() {
        Hidden = false;
    }

    // Hide the gaze cursor for specified frames
    virtual void HideCursorForFrames(const int hideFrames) {
        HiddenFrames = hideFrames;
    }

    // Sets an addition distance to offset the cursor for rendering. This can help avoid
    // z-fighting but also helps the cursor to feel more 3D by pushing it away from surfaces.
    virtual void SetDistanceOffset(float const offset) {
        DistanceOffset = offset;
    }

    virtual void SetTrailEnabled(bool const enabled);

   private:
    OvrGazeCursorInfo Info; // current cursor info
    OvrGazeCursorInfo RenderInfo; // latched info for rendering
    float CursorRotation; // current cursor rotation
    float RotationRateRadians; // rotation rate in radians
    float CursorScale; // scale of the cursor
    float DistanceOffset; // additional distance to offset towards the camera.
    int HiddenFrames; // Hide cursor for a number of frames
    Matrix4f CursorTransform[TRAIL_GHOSTS]; // transform for each ghost
    Matrix4f CursorScatterTransform[TRAIL_GHOSTS]; // transform for each depth-fail ghost
    int CurrentTransform; // the next CursorTransform[] to fill

    // Since ovrDrawSurface takes a pointer to a ovrSurfaceDef, we need to store a
    // ovrSurfaceDef somewhere. This means we have to update these surfaces inside of
    // the AppendSurfaceList function, which is otherwise const. This could be avoided if
    // we opt to copy the surface def instead of holding a pointer to it.
    mutable ovrSurfaceDef ZPassCursorSurface; // VBO for the cursor geometry that passes z test
    mutable ovrSurfaceDef ZFailCursorSurface; // VBO for the cursor geometry that fails z test
    VertexAttribs ZPassVertexAttribs; // stored so we only generate once and never reallocate
    VertexAttribs ZFailVertexAttribs; // stored so we only generate once and never reallocate

    GlTexture CursorTexture; // handle to the cursor's texture
    GlProgram CursorProgram; // vertex and pixel shaders for the cursor
    Vector4f CursorColorZPass;
    Vector4f CursorColorZFail;

    bool Initialized; // true once initialized
    bool Hidden; // true if the cursor should not render

   private:
    void UpdateCursorPositions(VertexAttribs& attr, Bounds3f& bounds, Matrix4f const* transforms)
        const;
};

GlGeometry CreateCursorGeometry(VertexAttribs& attr, int const numTrails, int const maxTrails);

//==============================
// OvrGazeCursorLocal::OvrGazeCursorLocal
OvrGazeCursorLocal::OvrGazeCursorLocal()
    : CursorRotation(0.0f),
      RotationRateRadians(0.0f),
      CursorScale(0.013f),
      DistanceOffset(0.05f),
      HiddenFrames(0),
      CurrentTransform(0),
      CursorTexture(),
      CursorColorZPass(),
      CursorColorZFail(),
      Initialized(false),
      Hidden(true) {}

//==============================
// OvrGazeCursorLocal::OvrGazeCursorLocal
OvrGazeCursorLocal::~OvrGazeCursorLocal() {}

//==============================
// OvrGazeCursorLocal::
void OvrGazeCursorLocal::Init(ovrFileSys& fileSys) {
    ALOG("OvrGazeCursorLocal::Init");
    // assert_WITH_TAG( Initialized == false, "GazeCursor" );

    if (Initialized) {
        ALOG("OvrGazeCursorLocal::Init - already initialized!");
        return;
    }

    {
        static ovrProgramParm cursorUniformParms[] = {
            {"UniformColor", ovrProgramParmType::FLOAT_VECTOR4},
            {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        };
        // Initialize cursors
        CursorProgram = GlProgram::Build(
            GazeCursorVertexSrc,
            GazeCursorFragmentSrc,
            cursorUniformParms,
            sizeof(cursorUniformParms) / sizeof(ovrProgramParm));

        CursorColorZFail = Vector4f(1.0f, 0.0f, 0.0f, 0.15f);
        CursorColorZPass = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

        int w = 0;
        int h = 0;
        CursorTexture = LoadTextureFromUri(
            fileSys, "apk:///res/raw/gaze_cursor_circle_64x64.ktx", TextureFlags_t(), w, h);

        // Z-Pass Surface
        ZPassCursorSurface.surfaceName = "gaze cursor Z-pass";
        ZPassCursorSurface.geo =
            CreateCursorGeometry(ZPassVertexAttribs, TRAIL_GHOSTS, TRAIL_GHOSTS);
        ZPassCursorSurface.graphicsCommand.Program = CursorProgram;
        ZPassCursorSurface.graphicsCommand.UniformData[0].Data = &CursorColorZPass;
        ZPassCursorSurface.graphicsCommand.UniformData[1].Data = &CursorTexture;

        ZPassCursorSurface.graphicsCommand.GpuState.blendEnable =
            ovrGpuState::BLEND_ENABLE_SEPARATE;
        ZPassCursorSurface.graphicsCommand.GpuState.blendMode = GL_FUNC_ADD;
        ZPassCursorSurface.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
        ZPassCursorSurface.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        ZPassCursorSurface.graphicsCommand.GpuState.blendSrcAlpha = GL_ONE;
        ZPassCursorSurface.graphicsCommand.GpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
        ZPassCursorSurface.graphicsCommand.GpuState.blendModeAlpha = GL_FUNC_ADD;
        ZPassCursorSurface.graphicsCommand.GpuState.depthFunc = GL_LEQUAL;

        ZPassCursorSurface.graphicsCommand.GpuState.frontFace = GL_CCW;
        ZPassCursorSurface.graphicsCommand.GpuState.depthEnable = true;
        ZPassCursorSurface.graphicsCommand.GpuState.depthMaskEnable = false;
        ZPassCursorSurface.graphicsCommand.GpuState.polygonOffsetEnable = false;
        ZPassCursorSurface.graphicsCommand.GpuState.cullEnable = false;

        // Z-Fail Surface
        ZFailCursorSurface.surfaceName = "gaze cursor Z-fail";
        ZFailCursorSurface.geo =
            CreateCursorGeometry(ZFailVertexAttribs, TRAIL_GHOSTS, TRAIL_GHOSTS);
        ZFailCursorSurface.graphicsCommand.Program = CursorProgram;
        ZFailCursorSurface.graphicsCommand.UniformData[0].Data = &CursorColorZFail;
        ZFailCursorSurface.graphicsCommand.UniformData[1].Data = &CursorTexture;

        ZFailCursorSurface.graphicsCommand.GpuState.blendEnable =
            ovrGpuState::BLEND_ENABLE_SEPARATE;
        ZFailCursorSurface.graphicsCommand.GpuState.blendMode = GL_FUNC_ADD;
        ZFailCursorSurface.graphicsCommand.GpuState.blendSrc = GL_SRC_ALPHA;
        ZFailCursorSurface.graphicsCommand.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        ZFailCursorSurface.graphicsCommand.GpuState.blendSrcAlpha = GL_ONE;
        ZFailCursorSurface.graphicsCommand.GpuState.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
        ZFailCursorSurface.graphicsCommand.GpuState.blendModeAlpha = GL_FUNC_ADD;
        ZFailCursorSurface.graphicsCommand.GpuState.depthFunc = GL_GREATER;

        ZFailCursorSurface.graphicsCommand.GpuState.frontFace = GL_CCW;
        ZFailCursorSurface.graphicsCommand.GpuState.depthEnable = true;
        ZFailCursorSurface.graphicsCommand.GpuState.depthMaskEnable = false;
        ZFailCursorSurface.graphicsCommand.GpuState.polygonOffsetEnable = false;
        ZFailCursorSurface.graphicsCommand.GpuState.cullEnable = false;
    }

    Initialized = true;
}

//==============================
// OvrGazeCursorLocal::
void OvrGazeCursorLocal::Shutdown() {
    ALOG("OvrGazeCursorLocal::Shutdown");
    // assert_WITH_TAG( Initialized == true, "GazeCursor" );

    ZPassCursorSurface.geo.Free();
    ZFailCursorSurface.geo.Free();

    DeleteTexture(CursorTexture);

    GlProgram::Free(CursorProgram);

    Initialized = false;
}

//==============================
// OvrGazeCursorLocal::UpdateDistance
void OvrGazeCursorLocal::UpdateDistance(float const d, eGazeCursorStateType const state) {
    // ALOG( "OvrGazeCursorLocal::UpdateDistance %.4f", d );
    if (d < Info.Distance) {
        // ALOG( "OvrGazeCursorLocal::UpdateDistance - new closest distace %.2f", d );
        Info.Distance = d;
        Info.State = state;
    }
}
//==============================
// OvrGazeCursorLocal::ForceDistance
void OvrGazeCursorLocal::ForceDistance(float const d, eGazeCursorStateType const state) {
    Info.Distance = d;
    Info.State = state;
}

//==============================
// OvrGazeCursorLocal::ClearGhosts
void OvrGazeCursorLocal::ClearGhosts() {
    CurrentTransform = 0;
}

static float frand() {
    return (rand() & 65535) / (65535.0f / 2.0f) - 1.0f;
}

//==============================
// OvrGazeCursorLocal::Frame
void OvrGazeCursorLocal::Frame(
    Matrix4f const& viewMatrix,
    Matrix4f const& traceMatrix,
    float const deltaTime) {
    // ALOG( "OvrGazeCursorLocal::Frame" );
    HiddenFrames -= 1;

    if (RotationRateRadians != 0.0f) // comparison to exactly 0 is intentional
    {
        CursorRotation += deltaTime * RotationRateRadians;
        if (CursorRotation > MATH_FLOAT_TWOPI) {
            CursorRotation -= MATH_FLOAT_TWOPI;
        } else if (CursorRotation < 0.0f) {
            CursorRotation += MATH_FLOAT_TWOPI;
        }
    }

    const Vector3f viewPos(GetViewMatrixPosition(viewMatrix));
    const Vector3f tracePos(traceMatrix.GetTranslation());
    const Vector3f traceFwd(GetViewMatrixForward(traceMatrix.Inverted()));

    Vector3f position = tracePos + traceFwd * (Info.Distance - DistanceOffset);

    Matrix4f viewRot = Matrix4f::LookAtRH(viewPos, position, GetViewMatrixUp(viewMatrix));

    viewRot.SetTranslation(Vector3f(0.0f));

    // Add one ghost for every four milliseconds.
    // Assume we are going to be at even multiples of vsync, so we don't need to bother
    // keeping an accurate roundoff count.
    const int lerps = static_cast<int>(deltaTime / 0.004f);

    const Matrix4f& prev = CursorTransform[CurrentTransform % TRAIL_GHOSTS];
    Matrix4f& now = CursorTransform[(CurrentTransform + lerps) % TRAIL_GHOSTS];

    now = Matrix4f::Translation(position) * viewRot.Inverted() *
        Matrix4f::RotationZ(CursorRotation) * Matrix4f::Scaling(CursorScale, CursorScale, 1.0f);

    if (CurrentTransform > 0) {
        for (int i = 1; i <= lerps; i++) {
            const float f = (float)i / lerps;
            Matrix4f& tween = CursorTransform[(CurrentTransform + i) % TRAIL_GHOSTS];

            // We only need to build a scatter on the final point that is already set by now
            if (i != lerps) {
                tween = ((now * f) + (prev * (1.0f - f)));
            }

            // When the cursor depth fails, draw a scattered set of ghosts
            Matrix4f& scatter = CursorScatterTransform[(CurrentTransform + i) % TRAIL_GHOSTS];

            // random point in circle
            float rx, ry;
            while (1) {
                rx = frand();
                ry = frand();
                if ((rx * rx + ry * ry < 1.0f)) {
                    break;
                }
            }
            scatter = tween * Matrix4f::Translation(rx, ry, 0.0f);
        }
    } else {
        // When CurrentTransform is 0, reset "lerp" cursors to the now transform as these will be
        // drawn in the next frame. If this is not done, only the cursor at pos 0 will have the now
        // orientation, while the others up to lerps will have old data causing a brief "duplicate"
        // to be on screen.
        for (int i = 1; i < lerps; i++) {
            Matrix4f& tween = CursorTransform[(CurrentTransform + i) % TRAIL_GHOSTS];
            tween = now;
        }
    }
    CurrentTransform += lerps;

    RenderInfo = Info;

    // Update cursor geometry.
    // Z-pass positions.
    UpdateCursorPositions(ZPassVertexAttribs, ZPassCursorSurface.geo.localBounds, CursorTransform);
    ZPassCursorSurface.geo.Update(ZPassVertexAttribs, false);

    // Z-fail positions.
    UpdateCursorPositions(
        ZFailVertexAttribs, ZFailCursorSurface.geo.localBounds, CursorScatterTransform);
    ZFailCursorSurface.geo.Update(ZFailVertexAttribs, false);
}

//==============================
// OvrGazeCursorLocal::AppendSurfaceList
void OvrGazeCursorLocal::AppendSurfaceList(std::vector<ovrDrawSurface>& surfaceList) const {
    // ALOG( "OvrGazeCursorLocal::AppendSurfaceList" );

    if (HiddenFrames >= 0) {
        return;
    }

    if (Hidden) {
        return;
    }

    if (CursorScale <= 0.0f) {
        ALOG("OvrGazeCursorLocal::AppendSurfaceList - scale 0");
        return;
    }

    surfaceList.push_back(ovrDrawSurface(&ZPassCursorSurface));
    ZPassCursorSurface.graphicsCommand.UniformData[1].Data = (void*)&CursorTexture;

    surfaceList.push_back(ovrDrawSurface(&ZFailCursorSurface));
    ZFailCursorSurface.graphicsCommand.UniformData[1].Data = (void*)&CursorTexture;
}

//==============================
// OvrGazeCursorLocal::GetInfo
OvrGazeCursorInfo OvrGazeCursorLocal::GetInfo() const {
    return Info;
}

//==============================
// OvrGazeCursorLocal::SetRotationRate
void OvrGazeCursorLocal::SetRotationRate(float const degreesPerSec) {
    RotationRateRadians = degreesPerSec * MATH_FLOAT_DEGREETORADFACTOR;
}

//==============================
// OvrGazeCursorLocal::SetCursorScale
void OvrGazeCursorLocal::SetCursorScale(float const scale) {
    CursorScale = scale;
}

//==============================
// OvrGazeCursorLocal::IsVisible
// Returns whether the gaze cursor will be drawn this frame
bool OvrGazeCursorLocal::IsVisible() const {
    if (HiddenFrames >= 0) {
        return false;
    }

    if (Hidden) {
        return false;
    }

    if (CursorScale <= 0.0f) {
        return false;
    }

    return true;
}

//==============================
// CreateCursorGeometry
GlGeometry CreateCursorGeometry(VertexAttribs& attr, int const numTrails, int const maxTrails) {
    static const Vector2f GazeCursorUV0s[4] = {
        Vector2f(0.0f, 0.0f),
        Vector2f(1.0f, 0.0f),
        Vector2f(0.0f, 1.0f),
        Vector2f(1.0f, 1.0f),
    };

    static const GLushort GazeCursorIndices[6] = {
        0,
        1,
        2,
        3,
        2,
        1,
    };

    for (int ghost = 0; ghost < numTrails; ghost++) {
        const float alpha = 1.0f - (ghost / (float)maxTrails);
        for (int vertexIndex = 0; vertexIndex < 4; vertexIndex++) {
            attr.position.push_back(Vector3f(0.0f, 0.0f, 0.0f));
            attr.uv0.push_back(GazeCursorUV0s[vertexIndex]);
            attr.color.push_back(Vector4f(1.0f, 1.0f, 1.0f, alpha));
        }
    }

    std::vector<TriangleIndex> indices;
    indices.resize(numTrails * 6);
    for (int ghost = 0; ghost < numTrails; ++ghost) {
        // push 6 indices for each quad "ghost"
        indices[ghost * 6 + 0] = static_cast<TriangleIndex>(ghost * 4 + GazeCursorIndices[0]);
        indices[ghost * 6 + 1] = static_cast<TriangleIndex>(ghost * 4 + GazeCursorIndices[1]);
        indices[ghost * 6 + 2] = static_cast<TriangleIndex>(ghost * 4 + GazeCursorIndices[2]);
        indices[ghost * 6 + 3] = static_cast<TriangleIndex>(ghost * 4 + GazeCursorIndices[3]);
        indices[ghost * 6 + 4] = static_cast<TriangleIndex>(ghost * 4 + GazeCursorIndices[4]);
        indices[ghost * 6 + 5] = static_cast<TriangleIndex>(ghost * 4 + GazeCursorIndices[5]);
    }

    return GlGeometry(attr, indices);
}

void OvrGazeCursorLocal::UpdateCursorPositions(
    VertexAttribs& attr,
    Bounds3f& bounds,
    Matrix4f const* transforms) const {
    const int numTrails = TRAIL_GHOSTS < CurrentTransform ? TRAIL_GHOSTS : CurrentTransform;

    // For missing trail transforms, draw degenerate triangles...
    for (int slice = numTrails; slice < TRAIL_GHOSTS; slice++) {
        for (int v = 0; v < 4; ++v) {
            attr.position[slice * 4 + v] = Vector3f(0.0f, 0.0f, 0.0f);
        }
    }

    static const Vector4f GazeCursorPositions[4] = {
        Vector4f(-1.0f, -1.0f, 0.0f, 1.0f),
        Vector4f(1.0f, -1.0f, 0.0f, 1.0f),
        Vector4f(-1.0f, 1.0f, 0.0f, 1.0f),
        Vector4f(1.0f, 1.0f, 0.0f, 1.0f),
    };

    bounds.Clear();

    // Transforming on the CPU shouldn't be too painful in this scenario since the vertex count is
    // low and we would be uploading the same amount of data in the form of transforms if we used
    // instancing anyways. So this costs us a few extra ops on the CPU, but allows us to bake the
    // color fade into a static VBO and avoids instancing which may or maynot be fast on all
    // hardware.
    for (int slice = numTrails - 1; slice >= 0; slice--) {
        const int index = (CurrentTransform - slice) % TRAIL_GHOSTS;
        const Matrix4f transform = transforms[index];
        for (int v = 0; v < 4; ++v) {
            Vector4f p(transform.Transform(GazeCursorPositions[v]));
            attr.position[slice * 4 + v] = Vector3f(p.x, p.y, p.z);
            bounds.AddPoint(Vector3f(p.x, p.y, p.z));
        }
    }
}

//==============================
// OvrGazeCursorLocal::SetTrailEnabled
void OvrGazeCursorLocal::SetTrailEnabled(bool const enabled) {
    const int numTrails = enabled ? TRAIL_GHOSTS : 1;
    if (ZPassCursorSurface.geo.vertexArrayObject != 0) {
        ZPassCursorSurface.geo.Free();
    }
    ZPassCursorSurface.geo = CreateCursorGeometry(ZPassVertexAttribs, numTrails, TRAIL_GHOSTS);

    if (ZFailCursorSurface.geo.vertexArrayObject != 0) {
        ZFailCursorSurface.geo.Free();
    }
    ZFailCursorSurface.geo = CreateCursorGeometry(ZFailVertexAttribs, numTrails, TRAIL_GHOSTS);
}

//==============================================================================================
//
// OvrGazeCursor
//
//==============================================================================================

//==============================
// OvrGazeCursor::Create
OvrGazeCursor* OvrGazeCursor::Create(ovrFileSys& fileSys) {
    OvrGazeCursorLocal* gc = new OvrGazeCursorLocal();
    gc->Init(fileSys);
    return gc;
}

//==============================
// OvrGazeCursor::Destroy
void OvrGazeCursor::Destroy(OvrGazeCursor*& gazeCursor) {
    if (gazeCursor != NULL) {
        static_cast<OvrGazeCursorLocal*>(gazeCursor)->Shutdown();
        delete gazeCursor;
        gazeCursor = NULL;
    }
}

} // namespace OVRFW
