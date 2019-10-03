//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2019 Guillaume Blanc                                         //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_IK_TWO_BONE_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_IK_TWO_BONE_JOB_H_

#include "../../base/platform.h"

#include "../../../../../../../../Common_3/OS/Math/MathTypes.h"

namespace ozz {
namespace animation {

// ozz::animation::IKTwoBoneJob performs inverse kinematic on a three joints
// chain (two bones).
// The job computes the transformations (rotations) that needs to be applied to
// the first two joints of the chain (named start and middle joints) such that
// the third joint (named end) reaches the provided target position (if
// possible). The job outputs start and middle joint rotation corrections as
// quaternions.
// The three joints must be ancestors, but don't need to be direct
// ancestors (joints in-between will simply remain fixed).
// Implementation is inspired by Autodesk Maya 2 bone IK, improved stability
// wise and extended with Soften IK.
struct IKTwoBoneJob {
  // Constructor, initializes default values.
  IKTwoBoneJob();

  // Validates job parameters. Returns true for a valid job, or false otherwise:
  // -if any input pointer is NULL
  // -if mid_axis isn't normalized.
  bool Validate() const;

  // Runs job's execution task.
  // The job is validated before any operation is performed, see Validate() for
  // more details.
  // Returns false if *this job is not valid.
  bool Run() const;

  // Job input.

  // Target IK position, in model-space. This is the position the end of the
  // joint chain will try to reach.
  Point3 target;

  // Normalized middle joint rotation axis, in middle joint local-space. Default
  // value is z axis. This axis is usually fixed for a given skeleton (as it's
  // in middle joint space). Its direction is defined like this: a positive
  // rotation around this axis will open the angle between the two bones. This
  // in turn also to define which side the two joints must bend. Job validation
  // will fail if mid_axis isn't normalized.
  Vector3 mid_axis;

  // Pole vector, in model-space. The pole vector defines the direction the
  // middle joint should point to, allowing to control IK chain orientation.
  // Note that IK chain orientation will flip when target vector and the pole
  // vector are aligned/crossing each other. It's caller responsibility to
  // ensure that this doesn't happen.
  Vector3 pole_vector;

  // Twist_angle rotates IK chain around the vector define by start-to-target
  // vector. Default is 0.
  float twist_angle;

  // Soften ratio allows the chain to gradually fall behind the target
  // position. This prevents the joint chain from snapping into the final
  // position, softening the final degrees before the joint chain becomes flat.
  // This ratio represents the distance to the end, from which softening is
  // starting.
  float soften;

  // Weight given to the IK correction clamped in range [0,1]. This allows to
  // blend / interpolate from no IK applied (0 weight) to full IK (1).
  float weight;

  // Model-space matrices of the start, middle and end joints of the chain.
  // The 3 joints should be ancestors. They don't need to be direct
  // ancestors though.
  const Matrix4* start_joint;
  const Matrix4* mid_joint;
  const Matrix4* end_joint;

  // Job output.

  // Local-space corrections to apply to start and middle joints in order for
  // end joint to reach target position.
  // These quaternions must be multiplied to the local-space quaternion of their
  // respective joints.
  Quat* start_joint_correction;
  Quat* mid_joint_correction;

  // Optional boolean output value, set to true if target can be reached with IK
  // computations. Reachability is driven by bone chain length, soften ratio and
  // target distance. Target is considered unreached if weight is less than 1.
  bool* reached;
};
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_IK_TWO_BONE_JOB_H_
