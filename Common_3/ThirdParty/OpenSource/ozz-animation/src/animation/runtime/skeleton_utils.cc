//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2017 Guillaume Blanc                                         //
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

//CONFFX_BEGIN
#include "../../../include/ozz/animation/runtime/skeleton_utils.h"

#include "../../../../../Common_3/OS/Math/MathTypes.h"

#include <assert.h>


namespace ozz {
namespace animation {

//CONFFX_BEGIN
// Unpacks skeleton bind pose stored in soa format by the skeleton.
	AffineTransform GetJointLocalBindPose(const Skeleton& _skeleton,
                                           int _joint) {
  assert(_joint >= 0 && _joint < _skeleton.num_joints() &&
         "Joint index out of range.");

  const SoaTransform& soa_transform =
      _skeleton.joint_bind_poses()[_joint / 4];

  // Transpose SoA data to AoS.
  Vector4 translations[4];
  transpose3x4(&soa_transform.translation.x, translations);
  Vector4 rotations[4];
  transpose4x4(&soa_transform.rotation.x, rotations);
  Vector4 scales[4];
  transpose3x4(&soa_transform.scale.x, scales);

  // Stores to the Transform object.
  AffineTransform bind_pose;
  const int offset = _joint % 4;
  translations[offset] = Vector4(bind_pose.translation, 0.f);
  rotations[offset] = Vector4(bind_pose.rotation);
  scales[offset] = Vector4(bind_pose.scale, 0.f);

  return bind_pose;
}
}  // namespace animation
}  // namespace ozz
//CONFFX_END
