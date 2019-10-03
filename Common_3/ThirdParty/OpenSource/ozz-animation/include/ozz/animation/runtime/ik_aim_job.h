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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_IK_AIM_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_IK_AIM_JOB_H_

#include "../../base/platform.h"

#include "../../../../../../../../Common_3/OS/Math/MathTypes.h"

namespace ozz {
// Forward declaration of math structures.
namespace math {
struct SimdQuaternion;
}

namespace animation {

// ozz::animation::IKAimJob rotates a joint so it aims at a target. Joint aim
// direction and up vectors can be different from joint axis. The job computes
// the transformation (rotation) that needs to be applied to the joints such
// that a provided forward vector (in joint local-space) aims at the target
// position (in skeleton model-space). Up vector (in joint local-space) is also
// used to keep the joint oriented in the same direction as the pole vector.
// The job also exposes an offset (in joint local-space) from where the forward
// vector should aim the target.
// Result is unstable if joint-to-target direction is parallel to pole vector,
// or if target is too close to joint position.
struct IKAimJob {
  // Default constructor, initializes default values.
  IKAimJob();

  // Validates job parameters. Returns true for a valid job, or false otherwise:
  // -if output quaternion pointer is NULL
  bool Validate() const;

  // Runs job's execution task.
  // The job is validated before any operation is performed, see Validate() for
  // more details.
  // Returns false if *this job is not valid.
  bool Run() const;

  // Job input.

  // Target position to aim at, in model-space
  Point3 target;

  // Joint forward axis, in joint local-space, to be aimed at target position.
  // This vector shall be normalized, otherwise validation will fail.
  // Default is x axis.
  Vector3 forward;

  // Offset position from the joint in local-space, that will aim at target.
  Vector3 offset;

  // Joint up axis, in joint local-space, used to keep the joint oriented in the
  // same direction as the pole vector. Default is y axis.
  Vector3 up;

  // Pole vector, in model-space. The pole vector defines the direction
  // the up should point to.  Note that IK chain orientation will flip when
  // target vector and the pole vector are aligned/crossing each other. It's
  // caller responsibility to ensure that this doesn't happen.
  Vector3 pole_vector;

  // Twist_angle rotates joint around the target vector.
  // Default is 0.
  float twist_angle;

  // Weight given to the IK correction clamped in range [0,1]. This allows to
  // blend / interpolate from no IK applied (0 weight) to full IK (1).
  float weight;

  // Joint model-space matrix.
  const Matrix4* joint;

  // Job output.

  // Output local-space joint correction quaternion. It needs to be multiplied
  // with joint local-space quaternion.
  Quat* joint_correction;

  // Optional boolean output value, set to true if target can be reached with IK
  // computations. Target is considered not reachable when target is between
  // joint and offset position.
  bool* reached;
};
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_IK_AIM_JOB_H_
