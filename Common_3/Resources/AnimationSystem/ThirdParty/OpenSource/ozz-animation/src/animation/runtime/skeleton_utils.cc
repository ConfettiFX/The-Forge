//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
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

#include "../../../include/ozz/animation/runtime/skeleton_utils.h"

#include <assert.h>

#include <cstring>

namespace ozz {
namespace animation {

int32_t FindJoint(const Skeleton& _skeleton, const char* _name) {
  const auto& names = _skeleton.joint_names();
  for (size_t i = 0; i < names.size(); ++i) {
    if (strcmp(names[i], _name) == 0) {
      return static_cast<int32_t>(i);
    }
  }
  return -1;
}

// Unpacks skeleton rest pose stored in soa format by the skeleton.
AffineTransform GetJointLocalRestPose(const Skeleton& _skeleton,
                                           int32_t _joint) {
  ASSERT(_joint >= 0 && _joint < _skeleton.num_joints() &&
         "Joint index out of range.");

  const SoaTransform& soa_transform =
      _skeleton.joint_rest_poses()[_joint / 4];

  // Transpose SoA data to AoS.
  Vector4 translations[4];
  transpose3x4(&soa_transform.translation.x, translations);
  Vector4 rotations[4];
  transpose4x4(&soa_transform.rotation.x, rotations);
  Vector4 scales[4];
  transpose3x4(&soa_transform.scale.x, scales);

  // Stores to the Transform object.
  AffineTransform rest_pose;
  const int32_t offset = _joint % 4;
  store3PtrU(translations[offset], (float*)&rest_pose.translation);
  storePtrU(rotations[offset], (float*)&rest_pose.rotation);
  store3PtrU(scales[offset], (float*)&rest_pose.scale);

  return rest_pose;
}
}  // namespace animation
}  // namespace ozz
