/********************************************************************************/ /**
 \file      OVR_Pose.h
 \brief     Implementation of 3D primitives such as vectors, matrices.
 \copyright Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
 *************************************************************************************/

#ifndef OVR_Pose_h
#define OVR_Pose_h

// This file is intended to be independent of the rest of LibOVRKernel and thus
// has no #include dependencies on either, other than OVR_Math.h which is also independent.

#include "OVR_Math.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant
#endif

//------------------------------------------------------------------------------------//
// ***** C Compatibility Types

// These declarations are used to support conversion between C types used in
// LibOVRKernel C interfaces and their C++ versions. As an example, they allow passing
// Vector3f into a function that expects ovrVector3f.

typedef struct ovrDynamicPosef_ ovrDynamicPosef;

namespace OVR {

// Forward-declare our templates.
template <class T>
class DynamicPose;

typedef struct ovrRigidBodyPosef_ ovrRigidBodyPosef;

// Specializations providing CompatibleTypes::Type value.
template <>
struct CompatibleTypes<DynamicPose<float>> {
    typedef ovrRigidBodyPosef Type;
};

// DynamicPose describes the complete pose, or a rigid body configuration, at a
// point in time, including first and second derivatives. It is used to specify
// instantaneous location and movement of the headset or other tracked object.
template <class T>
class DynamicPose {
   public:
    typedef typename CompatibleTypes<DynamicPose<T>>::Type CompatibleType;

    DynamicPose() : TimeInSeconds(0.0), PredictionInSeconds(0.0) {}
    // float <-> double conversion constructor.
    explicit DynamicPose(const DynamicPose<typename Math<T>::OtherFloatType>& src)
        : Pose(src.Pose),
          AngularVelocity(src.AngularVelocity),
          LinearVelocity(src.LinearVelocity),
          AngularAcceleration(src.AngularAcceleration),
          LinearAcceleration(src.LinearAcceleration),
          TimeInSeconds(src.TimeInSeconds),
          PredictionInSeconds(src.PredictionInSeconds) {}

    // C-interop support: DynamicPosef <-> ovrRigidBodyPosef
    DynamicPose(const typename CompatibleTypes<DynamicPose<T>>::Type& src)
        : Pose(src.Pose),
          AngularVelocity(src.AngularVelocity),
          LinearVelocity(src.LinearVelocity),
          AngularAcceleration(src.AngularAcceleration),
          LinearAcceleration(src.LinearAcceleration),
          TimeInSeconds(src.TimeInSeconds),
          PredictionInSeconds(src.PredictionInSeconds) {}

    DynamicPose(const Pose<T>& pose)
        : Pose(pose),
          AngularVelocity(Vector3<T>::Zero()),
          LinearVelocity(Vector3<T>::Zero()),
          AngularAcceleration(Vector3<T>::Zero()),
          LinearAcceleration(Vector3<T>::Zero()),
          TimeInSeconds(0.0),
          PredictionInSeconds(0.0) {}

    operator typename CompatibleTypes<DynamicPose<T>>::Type() const {
        CompatibleType result;
        result.Pose = Pose;
        result.AngularVelocity = AngularVelocity;
        result.LinearVelocity = LinearVelocity;
        result.AngularAcceleration = AngularAcceleration;
        result.LinearAcceleration = LinearAcceleration;
        result.TimeInSeconds = TimeInSeconds;
        result.PredictionInSeconds = PredictionInSeconds;
        return result;
    }

    static DynamicPose Identity() {
        DynamicPose<T> ret;
        ret.Pose = OVR::Pose<T>::Identity();
        ret.AngularVelocity = Vector3<T>::Zero();
        ret.LinearVelocity = Vector3<T>::Zero();
        ret.AngularAcceleration = Vector3<T>::Zero();
        ret.LinearAcceleration = Vector3<T>::Zero();
        return ret;
    }

    Pose<T> Pose;
    Vector3<T> AngularVelocity;
    Vector3<T> LinearVelocity;
    Vector3<T> AngularAcceleration;
    Vector3<T> LinearAcceleration;
#ifdef __i386__
    uint32_t padding;
#endif
    double TimeInSeconds; // Absolute time of this state sample.
    double PredictionInSeconds;
};

typedef DynamicPose<float> DynamicPosef;
typedef DynamicPose<float> RigidBodyPosef; // this name is deprecated

} // Namespace OVR

static_assert(sizeof(OVR::Posef) == 28, "unexpected OVR::Posef size");
static_assert(sizeof(OVR::DynamicPosef) == 96, "unexpected OVR::DynamicPosef size");

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
