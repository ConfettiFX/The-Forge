/************************************************************************************

Filename	:	VrCubeWorld_Framework.cpp
Content		:	This sample uses the application framework.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "Appl.h"
#include "System.h"
#include "GUI/GuiSys.h"
#include "Locale/OVR_Locale.h"
#include "Misc/Log.h"
#include <memory>
#include <vector>
#include <string>

/*
================================================================================

VrCubeWorld

================================================================================
*/

using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3d;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_INSTANCES = 1500;
static const int NUM_ROTATIONS = 16;
static const Vector4f CLEAR_COLOR(0.125f, 0.0f, 0.125f, 1.0f);

static const char VERTEX_SHADER[] = R"glsl(
in vec3 Position;
in vec4 VertexColor;
in mat4 VertexTransform;
out vec4 fragmentColor;
void main()
{
	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( VertexTransform * vec4( Position * 0.1, 1.0 ) ) );
	fragmentColor = VertexColor;
}
)glsl";

static const char FRAGMENT_SHADER[] = R"glsl(
in lowp vec4 fragmentColor;
void main()
{
	gl_FragColor = fragmentColor;
}
)glsl";

// setup Cube
struct ovrCubeVertices {
    Vector3f positions[8];
    Vector4f colors[8];
};

static ovrCubeVertices cubeVertices = {
    // positions
    {
        Vector3f(-1.0f, +1.0f, -1.0f),
        Vector3f(+1.0f, +1.0f, -1.0f),
        Vector3f(+1.0f, +1.0f, +1.0f),
        Vector3f(-1.0f, +1.0f, +1.0f), // top
        Vector3f(-1.0f, -1.0f, -1.0f),
        Vector3f(-1.0f, -1.0f, +1.0f),
        Vector3f(+1.0f, -1.0f, +1.0f),
        Vector3f(+1.0f, -1.0f, -1.0f) // bottom
    },
    // colors
    {Vector4f(1.0f, 0.0f, 1.0f, 1.0f),
     Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
     Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
     Vector4f(1.0f, 0.0f, 0.0f, 1.0f),
     Vector4f(0.0f, 0.0f, 1.0f, 1.0f),
     Vector4f(0.0f, 1.0f, 0.0f, 1.0f),
     Vector4f(1.0f, 0.0f, 1.0f, 1.0f),
     Vector4f(1.0f, 0.0f, 0.0f, 1.0f)},
};

static const unsigned short cubeIndices[36] = {
    0, 2, 1, 2, 0, 3, // top
    4, 6, 5, 6, 4, 7, // bottom
    2, 6, 7, 7, 1, 2, // right
    0, 4, 5, 5, 3, 0, // left
    3, 5, 6, 6, 2, 3, // front
    0, 1, 7, 7, 4, 0 // back
};

class VrCubeWorld : public ovrAppl {
   public:
    VrCubeWorld();
    virtual ~VrCubeWorld();

    // Called when the application initializes.
    // Must return true if the application initializes successfully.
    virtual bool AppInit(const OVRFW::ovrAppContext* context) override;
    // Called when the application shuts down
    virtual void AppShutdown(const OVRFW::ovrAppContext* context) override;
    // Called when the application is resumed by the system.
    virtual void AppResumed(const OVRFW::ovrAppContext* contet) override;
    // Called when the application is paused by the system.
    virtual void AppPaused(const OVRFW::ovrAppContext* context) override;
    // Called once per frame when the VR session is active.
    virtual OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn& in) override;
    // Called once per frame to allow the application to render eye buffers.
    virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
        override;
    // Called once per eye each frame for default renderer
    virtual void
    AppRenderEye(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye) override;

   private:
    ovrRenderState RenderState;
    ovrFileSys* FileSys;
    OvrGuiSys::SoundEffectPlayer* SoundEffectPlayer;
    OvrGuiSys* GuiSys;
    ovrLocale* Locale;
    ovrSurfaceRender SurfaceRender;

    ovrSurfaceDef SurfaceDef;
    unsigned int Random;
    GlProgram Program;
    GlGeometry Cube;
    GLint VertexTransformAttribute;
    GLuint InstanceTransformBuffer;
    ovrVector3f Rotations[NUM_ROTATIONS];
    ovrVector3f CubePositions[NUM_INSTANCES];
    int CubeRotations[NUM_INSTANCES];
    ovrMatrix4f CenterEyeViewMatrix;
    double startTime;

    float RandomFloat();
};

VrCubeWorld::VrCubeWorld()
    : ovrAppl(0, 0, CPU_LEVEL, GPU_LEVEL, true /* useMultiView */),
      SoundEffectPlayer(nullptr),
      GuiSys(nullptr),
      Locale(nullptr),
      Random(2) {
    CenterEyeViewMatrix = ovrMatrix4f_CreateIdentity();
}

VrCubeWorld::~VrCubeWorld() {
    delete SoundEffectPlayer;
    SoundEffectPlayer = nullptr;

    GlProgram::Free(Program);
    Cube.Free();
    GL(glDeleteBuffers(1, &InstanceTransformBuffer));

    OvrGuiSys::Destroy(GuiSys);
}

// Returns a random float in the range [0, 1].
float VrCubeWorld::RandomFloat() {
    Random = 1664525L * Random + 1013904223L;
    unsigned int rf = 0x3F800000 | (Random & 0x007FFFFF);
    return (*(float*)&rf) - 1.0f;
}

bool VrCubeWorld::AppInit(const OVRFW::ovrAppContext* context) {
    const ovrJava& jj = *(reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));
    const xrJava ctx = JavaContextConvert(jj);
    FileSys = OVRFW::ovrFileSys::Create(ctx);
    if (nullptr == FileSys) {
        ALOGE("Couldn't create FileSys");
        return false;
    }

    Locale = ovrLocale::Create(*ctx.Env, ctx.ActivityObject, "default");
    if (nullptr == Locale) {
        ALOGE("Couldn't create Locale");
        return false;
    }

    SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();
    if (nullptr == SoundEffectPlayer) {
        ALOGE("Couldn't create SoundEffectPlayer");
        return false;
    }

    GuiSys = OvrGuiSys::Create(&ctx);
    if (nullptr == GuiSys) {
        ALOGE("Couldn't create GUI");
        return false;
    }

    std::string fontName;
    Locale->GetLocalizedString("@string/font_name", "efigs.fnt", fontName);
    GuiSys->Init(FileSys, *SoundEffectPlayer, fontName.c_str(), nullptr);

    // Create the program.
    Program = GlProgram::Build(VERTEX_SHADER, FRAGMENT_SHADER, nullptr, 0);
    VertexTransformAttribute = glGetAttribLocation(Program.Program, "VertexTransform");

    // Create the cube.
    VertexAttribs attribs;
    attribs.position.resize(8);
    attribs.color.resize(8);
    for (int i = 0; i < 8; i++) {
        attribs.position[i] = cubeVertices.positions[i];
        attribs.color[i] = cubeVertices.colors[i];
    }

    std::vector<TriangleIndex> indices;
    indices.resize(36);
    for (int i = 0; i < 36; i++) {
        indices[i] = cubeIndices[i];
    }

    Cube.Create(attribs, indices);

    // Setup the instance transform attributes.
    GL(glBindVertexArray(Cube.vertexArrayObject));
    GL(glGenBuffers(1, &InstanceTransformBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformBuffer));
    GL(glBufferData(
        GL_ARRAY_BUFFER, NUM_INSTANCES * 4 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW));
    for (int i = 0; i < 4; i++) {
        GL(glEnableVertexAttribArray(VertexTransformAttribute + i));
        GL(glVertexAttribPointer(
            VertexTransformAttribute + i,
            4,
            GL_FLOAT,
            false,
            4 * 4 * sizeof(float),
            (void*)(i * 4 * sizeof(float))));
        GL(glVertexAttribDivisor(VertexTransformAttribute + i, 1));
    }
    GL(glBindVertexArray(0));

    // Setup random rotations.
    for (int i = 0; i < NUM_ROTATIONS; i++) {
        Rotations[i].x = RandomFloat();
        Rotations[i].y = RandomFloat();
        Rotations[i].z = RandomFloat();
    }

    // Setup random cube positions and rotations.
    for (int i = 0; i < NUM_INSTANCES; i++) {
        volatile float rx, ry, rz;
        for (;;) {
            rx = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));
            ry = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));
            rz = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));

            // If too close to 0,0,0
            if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f) {
                continue;
            }

            // Test for overlap with any of the existing cubes.
            bool overlap = false;
            for (int j = 0; j < i; j++) {
                if (fabsf(rx - CubePositions[j].x) < 4.0f &&
                    fabsf(ry - CubePositions[j].y) < 4.0f &&
                    fabsf(rz - CubePositions[j].z) < 4.0f) {
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
            const ovrVector3f* otherPos = &CubePositions[j - 1];
            const float otherDistSqr =
                otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
            if (distSqr > otherDistSqr) {
                insert = j;
                break;
            }
            CubePositions[j] = CubePositions[j - 1];
            CubeRotations[j] = CubeRotations[j - 1];
        }

        CubePositions[insert].x = rx;
        CubePositions[insert].y = ry;
        CubePositions[insert].z = rz;

        CubeRotations[insert] = (int)(RandomFloat() * (NUM_ROTATIONS - 0.1f));
    }

    // Create SurfaceDef
    SurfaceDef.surfaceName = "VrCubeWorld Framework";
    SurfaceDef.graphicsCommand.Program = Program;
    SurfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
    SurfaceDef.graphicsCommand.GpuState.cullEnable = true;
    SurfaceDef.graphicsCommand.GpuState.depthEnable = true;
    SurfaceDef.geo = Cube;
    SurfaceDef.numInstances = NUM_INSTANCES;

    SurfaceRender.Init();

    startTime = GetTimeInSeconds();

    return true;
}

void VrCubeWorld::AppShutdown(const OVRFW::ovrAppContext*) {
    ALOGV("AppShutdown - enter");
    SurfaceRender.Shutdown();
    OVRFW::ovrFileSys::Destroy(FileSys);
    RenderState = RENDER_STATE_ENDING;
    ALOGV("AppShutdown - exit");
}

void VrCubeWorld::AppResumed(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrSampleAppl::AppResumed");
    RenderState = RENDER_STATE_RUNNING;
}

void VrCubeWorld::AppPaused(const OVRFW::ovrAppContext* /* context */) {
    ALOGV("ovrSampleAppl::AppPaused");
}

OVRFW::ovrApplFrameOut VrCubeWorld::AppFrame(const OVRFW::ovrApplFrameIn& vrFrame) {
    // process input events first because this mirrors the behavior when OnKeyEvent was
    // a virtual function on VrAppInterface and was called by VrAppFramework.
    for (int i = 0; i < static_cast<int>(vrFrame.KeyEvents.size()); i++) {
        const int keyCode = vrFrame.KeyEvents[i].KeyCode;
        const int action = vrFrame.KeyEvents[i].Action;

        if (GuiSys->OnKeyEvent(keyCode, action)) {
            continue;
        }
    }

    Vector3f currentRotation;
    currentRotation.x = (float)(vrFrame.PredictedDisplayTime - startTime);
    currentRotation.y = (float)(vrFrame.PredictedDisplayTime - startTime);
    currentRotation.z = (float)(vrFrame.PredictedDisplayTime - startTime);

    ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
    for (int i = 0; i < NUM_ROTATIONS; i++) {
        rotationMatrices[i] = ovrMatrix4f_CreateRotation(
            Rotations[i].x * currentRotation.x,
            Rotations[i].y * currentRotation.y,
            Rotations[i].z * currentRotation.z);
    }

    // Update the instance transform attributes.
    GL(glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformBuffer));
    GL(Matrix4f* cubeTransforms = (Matrix4f*)glMapBufferRange(
           GL_ARRAY_BUFFER,
           0,
           NUM_INSTANCES * sizeof(Matrix4f),
           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    for (int i = 0; i < NUM_INSTANCES; i++) {
        const int index = CubeRotations[i];

        // Write in order in case the mapped buffer lives on write-combined memory.
        cubeTransforms[i].M[0][0] = rotationMatrices[index].M[0][0];
        cubeTransforms[i].M[0][1] = rotationMatrices[index].M[0][1];
        cubeTransforms[i].M[0][2] = rotationMatrices[index].M[0][2];
        cubeTransforms[i].M[0][3] = rotationMatrices[index].M[0][3];

        cubeTransforms[i].M[1][0] = rotationMatrices[index].M[1][0];
        cubeTransforms[i].M[1][1] = rotationMatrices[index].M[1][1];
        cubeTransforms[i].M[1][2] = rotationMatrices[index].M[1][2];
        cubeTransforms[i].M[1][3] = rotationMatrices[index].M[1][3];

        cubeTransforms[i].M[2][0] = rotationMatrices[index].M[2][0];
        cubeTransforms[i].M[2][1] = rotationMatrices[index].M[2][1];
        cubeTransforms[i].M[2][2] = rotationMatrices[index].M[2][2];
        cubeTransforms[i].M[2][3] = rotationMatrices[index].M[2][3];

        cubeTransforms[i].M[3][0] = CubePositions[i].x;
        cubeTransforms[i].M[3][1] = CubePositions[i].y;
        cubeTransforms[i].M[3][2] = CubePositions[i].z;
        cubeTransforms[i].M[3][3] = 1.0f;
    }
    GL(glUnmapBuffer(GL_ARRAY_BUFFER));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    CenterEyeViewMatrix = OVR::Matrix4f(vrFrame.HeadPose);

    // Update GUI systems last, but before rendering anything.
    GuiSys->Frame(vrFrame, CenterEyeViewMatrix);

    return OVRFW::ovrApplFrameOut();
}

void VrCubeWorld::AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
    switch (RenderState) {
        case RENDER_STATE_LOADING: {
            DefaultRenderFrame_Loading(in, out);
        } break;
        case RENDER_STATE_RUNNING: {
            {
                /// Frame matrices
                out.FrameMatrices.CenterView = CenterEyeViewMatrix;
                for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
                    out.FrameMatrices.EyeView[eye] = in.Eye[eye].ViewMatrix;
                    // Calculate projection matrix using custom near plane value.
                    out.FrameMatrices.EyeProjection[eye] = ovrMatrix4f_CreateProjectionFov(
                        SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, 0.0f, 0.0f, 0.1f, 0.0f);
                }

                /// Surface
                out.Surfaces.push_back(ovrDrawSurface(&SurfaceDef));

                // Append GuiSys surfaces.
                GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);

                ///	worldLayer.Header.Flags |=
                /// VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
            }
            DefaultRenderFrame_Running(in, out);
        } break;
        case RENDER_STATE_ENDING: {
            DefaultRenderFrame_Ending(in, out);
        } break;
    }
}

void VrCubeWorld::AppRenderEye(
    const OVRFW::ovrApplFrameIn& in,
    OVRFW::ovrRendererOutput& out,
    int eye) {
    // Render the surfaces returned by Frame.
    SurfaceRender.RenderSurfaceList(
        out.Surfaces,
        out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
        out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
        eye);
}

} // namespace OVRFW

//==============================================================
// android_main
//==============================================================
void android_main(struct android_app* app) {
    std::unique_ptr<OVRFW::VrCubeWorld> appl =
        std::unique_ptr<OVRFW::VrCubeWorld>(new OVRFW::VrCubeWorld());
    appl->Run(app);
}
