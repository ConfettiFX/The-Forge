/************************************************************************************

Filename    :   ParticleSystem.h
Content     :   A simple particle system for System Activities.
Created     :   October 12, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include <vector>

#include "OVR_Math.h"
#include "OVR_TypesafeNumber.h"

#include "FrameParams.h"
#include "Render/SurfaceRender.h"
#include "Render/GlProgram.h"
#include "OVR_FileSys.h"

#include "EaseFunctions.h"

#include <vector>
#include <string>

namespace OVRFW {

template <typename T>
class ovrSimpleArray {
   public:
    ovrSimpleArray() {}

    void PushBack(T const& item) {
        Vector.push_back(item);
    }

    void PopBack() {
        Vector.pop_back();
    }

    void Reserve(int const newCapacity) {
        Vector.reserve(newCapacity);
    }

    void Resize(int const newCount) {
        Vector.resize(newCount);
    }

    void RemoveAtUnordered(int const index) {
        Vector[index] = Vector[Vector.size() - 1];
        Vector.pop_back();
    }

    int GetSizeI() const {
        return static_cast<int>(Vector.size());
    }
    size_t GetSize() const {
        return Vector.size();
    }
    int GetCapacity() const {
        return Vector.capacity();
    }

    T const* GetDataPtr() const {
        return &Vector[0];
    }
    T* GetDataPtr() {
        return &Vector[0];
    }

    T const& operator[](int const index) const {
        return Vector[index];
    }
    T& operator[](int const index) {
        return Vector[index];
    }

   private:
    std::vector<T> Vector;
};

struct ovrVertexAttribs {
    ovrSimpleArray<OVR::Vector3f> position;
    ovrSimpleArray<OVR::Vector3f> normal;
    ovrSimpleArray<OVR::Vector3f> tangent;
    ovrSimpleArray<OVR::Vector3f> binormal;
    ovrSimpleArray<OVR::Vector4f> color;
    ovrSimpleArray<OVR::Vector2f> uv0;
    ovrSimpleArray<OVR::Vector2f> uv1;
    ovrSimpleArray<OVR::Vector4i> jointIndices;
    ovrSimpleArray<OVR::Vector4f> jointWeights;
};

class ovrTextureAtlas;

struct particleDerived_t {
    OVR::Vector3f Pos;
    OVR::Vector4f Color;
    float Orientation; // roll angle in radians
    float Scale;
    uint16_t SpriteIndex;
};

struct particleSort_t {
    int ActiveIndex;
    float DistanceSq;
};

//==============================================================
// ovrParticleSystem
class ovrParticleSystem {
   public:
    enum ovrParticleIndex { INVALID_PARTICLE_INDEX = -1 };

    typedef OVR::TypesafeNumberT<int32_t, ovrParticleIndex, INVALID_PARTICLE_INDEX> handle_t;

    ovrParticleSystem();
    virtual ~ovrParticleSystem();

    // specify sprite locations as a regular grid
    void Init(
        const int maxParticles,
        const ovrTextureAtlas* atlas,
        const ovrGpuState& gpuState,
        bool const sortParticles);

    void Frame(
        const OVRFW::ovrApplFrameIn& frame,
        const ovrTextureAtlas* textureAtlas,
        const OVR::Matrix4f& centerEyeViewMatrix);

    void Shutdown();

    void RenderEyeView(
        OVR::Matrix4f const& viewMatrix,
        OVR::Matrix4f const& projectionMatrix,
        std::vector<ovrDrawSurface>& surfaceList) const;

    handle_t AddParticle(
        const OVRFW::ovrApplFrameIn& frame,
        const OVR::Vector3f& initialPosition,
        const float initialOrientation,
        const OVR::Vector3f& initialVelocity,
        const OVR::Vector3f& acceleration,
        const OVR::Vector4f& initialColor,
        const ovrEaseFunc easeFunc,
        const float rotationRate,
        const float scale,
        const float lifeTime,
        const uint16_t spriteIndex);

    void UpdateParticle(
        const OVRFW::ovrApplFrameIn& frame,
        const handle_t handle,
        const OVR::Vector3f& position,
        const float orientation,
        const OVR::Vector3f& velocity,
        const OVR::Vector3f& acceleration,
        const OVR::Vector4f& color,
        const ovrEaseFunc easeFunc,
        const float rotationRate,
        const float scale,
        const float lifeTime,
        const uint16_t spriteIndex);

    void RemoveParticle(const handle_t handle);

    static ovrGpuState GetDefaultGpuState();

   private:
    void CreateGeometry(const int maxParticles);

    int GetMaxParticles() const {
        return SurfaceDef.geo.vertexCount / 4;
    }

    class ovrParticle {
       public:
        // empty constructor so we don't pay the price for double initialization
        ovrParticle() {}

        double StartTime; // time particle was created, negative means this particle is invalid
        float LifeTime; // time particle should die
        OVR::Vector3f InitialPosition; // initial position of the particle
        float InitialOrientation; // initial orientation of the particle
        OVR::Vector3f InitialVelocity; // initial velocity of the particle
        OVR::Vector3f HalfAcceleration; // 1/2 the initial acceleration of the particle
        OVR::Vector4f InitialColor; // initial color of the particle
        float RotationRate; // rotation of the particle
        float InitialScale; // initial scale of the particle
        uint16_t SpriteIndex; // index of the sprite for this particle
        ovrEaseFunc EaseFunc; // parametric function used to compute alpha
    };

    int MaxParticles; // maximum allowd particles
    ovrSimpleArray<ovrParticle> Particles; // all active particles
    ovrSimpleArray<handle_t> FreeParticles; // indices of free particles
    ovrSimpleArray<handle_t> ActiveParticles; // indices of active particles
    ovrSimpleArray<particleDerived_t> Derived;
    ovrSimpleArray<particleSort_t> SortIndices;
    ovrSimpleArray<uint8_t> PackedAttr;
    ovrVertexAttribs Attr;
    GlProgram Program;
    ovrSurfaceDef SurfaceDef;
    OVR::Matrix4f ModelMatrix;
    bool SortParticles;
};

} // namespace OVRFW
