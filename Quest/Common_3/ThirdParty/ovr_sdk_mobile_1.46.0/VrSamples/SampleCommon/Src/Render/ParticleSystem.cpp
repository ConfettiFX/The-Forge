/************************************************************************************

Filename    :   ParticleSystem.h
Content     :   A simple particle system for System Activities.
Created     :   October 12, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "ParticleSystem.h"

#include "TextureAtlas.h"
#include "Render/GeometryBuilder.h"
#include "Render/GlGeometry.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

inline Vector3f GetViewMatrixForward(Matrix4f const& m) {
    return Vector3f(-m.M[2][0], -m.M[2][1], -m.M[2][2]).Normalized();
}

namespace OVRFW {

static const char* particleVertexSrc = R"glsl(
attribute vec4 Position;
attribute vec2 TexCoord;
attribute vec4 VertexColor;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
    gl_Position = TransformVertex( Position );
    oTexCoord = TexCoord;
    oColor = VertexColor;
}
)glsl";

static const char* particleFragmentSrc = R"glsl(
uniform sampler2D Texture0;
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
    gl_FragColor = oColor * texture2D( Texture0, oTexCoord );
}
)glsl";

static const char* particleGeoFragmentSrc = R"glsl(
varying highp vec2 oTexCoord;
varying lowp vec4 oColor;
void main()
{
    float dist = distance(oTexCoord, vec2(0.0f));
    float alpha = smoothstep(0.6f, 0.35f, dist);
    gl_FragColor = mix(vec4(0.0f), oColor, alpha);
}
)glsl";

static Vector3f quadVertPos[4] = {
    {-0.5f, 0.5f, 0.0f},
    {0.5f, 0.5f, 0.0f},
    {0.5f, -0.5f, 0.0f},
    {-0.5f, -0.5f, 0.0f}};

static Vector2f quadUVs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

ovrParticleSystem::ovrParticleSystem() : MaxParticles(0) {}

ovrParticleSystem::~ovrParticleSystem() {
    Shutdown();
}

void ovrParticleSystem::Init(
    const int maxParticles,
    const ovrTextureAtlas* atlas,
    const ovrGpuState& gpuState,
    bool const sortParticles) {
    // this can be called multiple times
    Shutdown();

    MaxParticles = maxParticles;

    // free any existing particles
    Particles.Reserve(maxParticles);
    FreeParticles.Reserve(maxParticles);
    ActiveParticles.Reserve(maxParticles);

    // create the geometry
    CreateGeometry(maxParticles);

    {
        OVRFW::ovrProgramParm uniformParms[] = {
            /// Vertex
            /// Fragment
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        const int uniformCount = sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm);
        if (atlas != nullptr) {
            Program = OVRFW::GlProgram::Build(
                particleVertexSrc, particleFragmentSrc, uniformParms, uniformCount);
            SurfaceDef.surfaceName = std::string("particles_") + atlas->GetTextureName();
            SurfaceDef.graphicsCommand.Textures[0] = atlas->GetTexture();
        } else {
            Program = OVRFW::GlProgram::Build(
                particleVertexSrc, particleGeoFragmentSrc, uniformParms, uniformCount);
        }
    }

    SurfaceDef.graphicsCommand.Program = Program;
    SurfaceDef.graphicsCommand.BindUniformTextures();

    SurfaceDef.graphicsCommand.GpuState = gpuState;

    SortParticles = sortParticles;

    Derived.Reserve(maxParticles);
    SortIndices.Reserve(maxParticles);
    Attr.position.Reserve(maxParticles * 4);
    Attr.color.Reserve(maxParticles * 4);
    Attr.uv0.Reserve(maxParticles * 4);
    PackedAttr.Reserve(maxParticles * 12 * 16 * 8);
}

ovrGpuState ovrParticleSystem::GetDefaultGpuState() {
    ovrGpuState s;
    s.blendEnable = ovrGpuState::BLEND_ENABLE;
    s.blendSrc = GL_SRC_ALPHA;
    s.blendDst = GL_ONE;
    s.depthEnable = true;
    s.depthMaskEnable = false;
    s.cullEnable = true;
    return s;
}

int ParticleSortFn(void const* a, void const* b) {
    if (static_cast<const particleSort_t*>(b)->DistanceSq <
        static_cast<const particleSort_t*>(a)->DistanceSq) {
        return -1;
    }
    return 1;
}

template <typename _attrib_type_>
void PackVertexAttribute2(
    ovrSimpleArray<uint8_t>& packed,
    const ovrSimpleArray<_attrib_type_>& attrib,
    const int glLocation,
    const int glType,
    const int glComponents) {
    if (attrib.GetSize() > 0) {
        const int offset = packed.GetSizeI();
        const int size = attrib.GetSizeI() * sizeof(attrib[0]);

        packed.Resize(offset + size);
        memcpy(&packed[offset], attrib.GetDataPtr(), size);

        glEnableVertexAttribArray(glLocation);
        glVertexAttribPointer(
            glLocation, glComponents, glType, false, sizeof(attrib[0]), (void*)((size_t)offset));
    } else {
        glDisableVertexAttribArray(glLocation);
    }
}

void UpdateGeometry(
    GlGeometry& geo,
    ovrSimpleArray<uint8_t>& packed,
    const ovrVertexAttribs& attribs) {
    geo.vertexCount = attribs.position.GetSizeI();

    glBindVertexArray(geo.vertexArrayObject);

    glBindBuffer(GL_ARRAY_BUFFER, geo.vertexBuffer);

    PackVertexAttribute2(packed, attribs.position, VERTEX_ATTRIBUTE_LOCATION_POSITION, GL_FLOAT, 3);
    PackVertexAttribute2(packed, attribs.normal, VERTEX_ATTRIBUTE_LOCATION_NORMAL, GL_FLOAT, 3);
    PackVertexAttribute2(packed, attribs.tangent, VERTEX_ATTRIBUTE_LOCATION_TANGENT, GL_FLOAT, 3);
    PackVertexAttribute2(packed, attribs.binormal, VERTEX_ATTRIBUTE_LOCATION_BINORMAL, GL_FLOAT, 3);
    PackVertexAttribute2(packed, attribs.color, VERTEX_ATTRIBUTE_LOCATION_COLOR, GL_FLOAT, 4);
    PackVertexAttribute2(packed, attribs.uv0, VERTEX_ATTRIBUTE_LOCATION_UV0, GL_FLOAT, 2);
    PackVertexAttribute2(packed, attribs.uv1, VERTEX_ATTRIBUTE_LOCATION_UV1, GL_FLOAT, 2);
    PackVertexAttribute2(
        packed, attribs.jointIndices, VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES, GL_INT, 4);
    PackVertexAttribute2(
        packed, attribs.jointWeights, VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS, GL_FLOAT, 4);

    size_t s = packed.GetSize() * sizeof(packed[0]);
    glBufferData(
        GL_ARRAY_BUFFER,
        s,
        packed.GetDataPtr(),
        GL_STATIC_DRAW); // shouldn't this be GL_DYNAMIC_DRAW?
}

void ovrParticleSystem::Frame(
    const OVRFW::ovrApplFrameIn& frame,
    const ovrTextureAtlas* atlas,
    const Matrix4f& centerEyeViewMatrix) {
    // OVR_PERF_TIMER( ovrParticleSystem_Frame );

    SurfaceDef.geo.indexCount = 0;
    if (ActiveParticles.GetSizeI() <= 0) {
        return;
    }

    // update particles
    Matrix4f invViewMatrix = centerEyeViewMatrix.Inverted();
    Vector3f viewPos = invViewMatrix.GetTranslation();

    int activeCount = 0;

    // update existing particles and its vertices, deriving the current state of each particle based
    // on it's current age store the derived state of each particle in the derived array. Also, make
    // another array that is just the index of each particle and its distance to the view position.
    // This array will be sorted by distance and then used to index into the derived array the
    // vertices are transformed.

    ovrSimpleArray<particleDerived_t>& derived = Derived;
    ovrSimpleArray<particleSort_t>& sortIndices = SortIndices;
    derived.Resize(ActiveParticles.GetSizeI());
    sortIndices.Resize(ActiveParticles.GetSizeI());

    for (int i = 0; i < ActiveParticles.GetSizeI(); ++i) {
        const handle_t handle = ActiveParticles[i];
        ovrParticle& p = Particles[handle.Get()];

        if (frame.PredictedDisplayTime - p.StartTime > p.LifeTime) {
            // free expired particle
            p.StartTime = -1.0; // mark as unused
            FreeParticles.PushBack(handle);
            ActiveParticles.RemoveAtUnordered(i);
            i--; // last particle was moved into current slot, so don't skip it
            continue;
        }

        float t = static_cast<float>(frame.PredictedDisplayTime - p.StartTime);
        float tSq = t * t;

        particleDerived_t& d = derived[activeCount];
        // x = x0 + v0 * t + 0.5f * a * t^2
        d.Pos = p.InitialPosition + p.InitialVelocity * t + p.HalfAcceleration * tSq;
        d.Orientation = (p.RotationRate * t) + p.InitialOrientation;

        d.Color = EaseFunctions[p.EaseFunc](p.InitialColor, t / p.LifeTime);

        d.Scale = p.InitialScale;
        d.SpriteIndex = p.SpriteIndex;

        particleSort_t& ps = sortIndices[activeCount];
        ps.ActiveIndex = activeCount;
        ps.DistanceSq =
            (d.Pos - viewPos).LengthSq(); // Dot( centerEyeViewMatrix.GetZBasis() );//.LengthSq();

        activeCount++;
    }

    assert(ActiveParticles.GetSizeI() == activeCount);

    if (activeCount > 0) {
        // sort by distance to view pos
        if (SortParticles) {
            qsort(&sortIndices[0], activeCount, sizeof(sortIndices[0]), ParticleSortFn);
        }

        Attr.position.Resize(activeCount * 4);
        Attr.color.Resize(activeCount * 4);
        Attr.uv0.Resize(activeCount * 4);

        // transform vertices for each particle quad
        for (int i = 0; i < activeCount; ++i) {
            particleSort_t const& si = sortIndices[i];
            particleDerived_t const& p = derived[si.ActiveIndex];

            // transform vertices on the CPU
            Matrix4f rotMatrix = Matrix4f::RotationZ(p.Orientation);
            // This always aligns the particle to the direction of the particle to the view
            // position. This looks a little better but is more expensive and only really makes a
            // difference for large particles.
            Vector3f normal = (viewPos - p.Pos).Normalized();
            if (normal.LengthSq() < 0.999f) {
                normal = GetViewMatrixForward(centerEyeViewMatrix);
            }
            Matrix4f particleTransform =
                Matrix4f::CreateFromBasisVectors(normal, Vector3f(0.0f, 1.0f, 0.0f));
            particleTransform.SetTranslation(p.Pos);

            for (int v = 0; v < 4; ++v) {
                Attr.position[i * 4 + v] =
                    particleTransform.Transform(rotMatrix.Transform(quadVertPos[v] * p.Scale));
                Attr.color[i * 4 + v] = p.Color;
            }

            if (atlas != nullptr) {
                // set UVs of this sprite in the atlas
                const ovrTextureAtlas::ovrSpriteDef& sd = atlas->GetSpriteDef(p.SpriteIndex);
                Attr.uv0[i * 4 + 0] = Vector2f(sd.uvMins.x, sd.uvMins.y);
                Attr.uv0[i * 4 + 1] = Vector2f(sd.uvMaxs.x, sd.uvMins.y);
                Attr.uv0[i * 4 + 2] = Vector2f(sd.uvMaxs.x, sd.uvMaxs.y);
                Attr.uv0[i * 4 + 3] = Vector2f(sd.uvMins.x, sd.uvMaxs.y);
            } else {
                Attr.uv0[i * 4 + 0] = Vector2f(-1, -1);
                Attr.uv0[i * 4 + 1] = Vector2f(1, -1);
                Attr.uv0[i * 4 + 2] = Vector2f(1, 1);
                Attr.uv0[i * 4 + 3] = Vector2f(-1, 1);
            }
        }
    }

    // update the geometry with new vertex attributes
    SurfaceDef.geo.indexCount = activeCount * 6;
    PackedAttr.Resize(0);
    UpdateGeometry(SurfaceDef.geo, PackedAttr, Attr);
}

void ovrParticleSystem::Shutdown() {
    SurfaceDef.geo.Free();
    OVRFW::GlProgram::Free(Program);
}

void ovrParticleSystem::RenderEyeView(
    Matrix4f const& viewMatrix,
    Matrix4f const& projectionMatrix,
    std::vector<ovrDrawSurface>& surfaceList) const {
    // OVR_UNUSED( viewMatrix );
    // OVR_UNUSED( projectionMatrix );

    // add a surface
    ovrDrawSurface surf;
    surf.modelMatrix = ModelMatrix;
    surf.surface = &SurfaceDef;
    surfaceList.push_back(surf);
}

ovrParticleSystem::handle_t ovrParticleSystem::AddParticle(
    const OVRFW::ovrApplFrameIn& frame,
    const Vector3f& initialPosition,
    const float initialOrientation,
    const Vector3f& initialVelocity,
    const Vector3f& acceleration,
    const Vector4f& initialColor,
    const ovrEaseFunc easeFunc,
    const float rotationRate,
    const float scale,
    const float lifeTime,
    const uint16_t spriteIndex) {
    ovrParticle* p;

    handle_t particleHandle;
    if (FreeParticles.GetSizeI() > 0) {
        particleHandle = handle_t(FreeParticles[FreeParticles.GetSizeI() - 1]);
        FreeParticles.PopBack();
        ActiveParticles.PushBack(particleHandle);
        assert(particleHandle.IsValid());
        assert(particleHandle.Get() < Particles.GetSizeI());
        p = &Particles[particleHandle.Get()];
    } else {
        if (Particles.GetSizeI() >= MaxParticles) {
            return handle_t(); // adding more would overflow the VAO
        }
        particleHandle = handle_t(Particles.GetSizeI());
        Particles.PushBack(ovrParticle());
        ActiveParticles.PushBack(particleHandle);
        p = &Particles[Particles.GetSizeI() - 1];
    }

    p->StartTime = frame.PredictedDisplayTime;
    p->LifeTime = lifeTime;
    p->InitialPosition = initialPosition;
    p->InitialOrientation = initialOrientation;
    p->InitialVelocity = initialVelocity;
    p->HalfAcceleration = acceleration * 0.5f;
    p->InitialColor = initialColor;
    p->EaseFunc = easeFunc;
    p->RotationRate = rotationRate;
    p->InitialScale = scale;
    p->SpriteIndex = spriteIndex;

    return particleHandle;
}

void ovrParticleSystem::UpdateParticle(
    const OVRFW::ovrApplFrameIn& frame,
    const handle_t handle,
    const Vector3f& position,
    const float orientation,
    const Vector3f& velocity,
    const Vector3f& acceleration,
    const Vector4f& color,
    const ovrEaseFunc easeFunc,
    const float rotationRate,
    const float scale,
    const float lifeTime,
    const uint16_t spriteIndex) {
    if (!handle.IsValid() || handle.Get() >= Particles.GetSizeI()) {
        assert(handle.IsValid() && handle.Get() < Particles.GetSizeI());
        return;
    }
    ovrParticle& p = Particles[handle.Get()];
    p.InitialPosition = position;
    p.InitialOrientation = orientation;
    p.InitialVelocity = velocity;
    p.HalfAcceleration = acceleration * 0.5f;
    p.InitialColor = color;
    p.EaseFunc = easeFunc;
    p.RotationRate = rotationRate;
    p.InitialScale = scale;
    p.SpriteIndex = spriteIndex;
    p.StartTime = frame.PredictedDisplayTime;
    p.LifeTime = lifeTime;
}

void ovrParticleSystem::RemoveParticle(const handle_t handle) {
    if (!handle.IsValid() || handle.Get() >= Particles.GetSizeI()) {
        return;
    }
    // particle will get removed in the next update
    Particles[handle.Get()].StartTime = -1.0; // mark as unused
    Particles[handle.Get()].LifeTime = 0.0;
}

void ovrParticleSystem::CreateGeometry(const int maxParticles) {
    SurfaceDef.geo.Free();

    VertexAttribs attr;
    const int numVerts = maxParticles * 4;

    attr.position.resize(numVerts);
    attr.normal.resize(numVerts);
    attr.color.resize(numVerts);
    attr.uv0.resize(numVerts);

    std::vector<TriangleIndex> indices;
    const int numIndices = maxParticles * 6;
    indices.resize(numIndices);

    for (int i = 0; i < maxParticles; ++i) {
        for (int v = 0; v < 4; v++) {
            attr.position[i * 4 + v] = quadVertPos[v];
            attr.normal[i * 4 + v] = {0.0f, 0.0f, 1.0f};
            attr.color[i * 4 + v] = {1.0f, 0.0f, 1.0f, 1.0f};
            attr.uv0[i * 4 + v] = quadUVs[v];
        }

        indices[i * 6 + 0] = static_cast<uint16_t>(i * 4 + 0);
        indices[i * 6 + 1] = static_cast<uint16_t>(i * 4 + 3);
        indices[i * 6 + 2] = static_cast<uint16_t>(i * 4 + 1);
        indices[i * 6 + 3] = static_cast<uint16_t>(i * 4 + 1);
        indices[i * 6 + 4] = static_cast<uint16_t>(i * 4 + 3);
        indices[i * 6 + 5] = static_cast<uint16_t>(i * 4 + 2);
    }

    SurfaceDef.geo.Create(attr, indices);
    SurfaceDef.geo.indexCount = 0; // nothing to render until particles are added
}

} // namespace OVRFW
