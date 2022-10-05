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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_



#include "../../../../include/ozz/animation/runtime/export.h"
#include "../../../../include/ozz/animation/runtime/skeleton.h"

namespace ozz {
namespace animation {

// Get rest-pose of a skeleton joint.
OZZ_ANIMATION_DLL AffineTransform GetJointLocalRestPose(
    const Skeleton& _skeleton, int32_t _joint);

// Test if a joint is a leaf. _joint number must be in range [0, num joints].
// "_joint" is a leaf if it's the last joint, or next joint's parent isn't
// "_joint".
inline bool IsLeaf(const Skeleton& _skeleton, int32_t _joint) {
  const int32_t num_joints = _skeleton.num_joints();
  ASSERT(_joint >= 0 && _joint < num_joints && "_joint index out of range");
  const span<const int16_t>& parents = _skeleton.joint_parents();
  const int32_t next = _joint + 1;
  return next == num_joints || parents[next] != _joint;
}

// Finds joint index by name. Uses a case sensitive comparison.
OZZ_ANIMATION_DLL int32_t FindJoint(const Skeleton& _skeleton, const char* _name);

// Applies a specified functor to each joint in a depth-first order.
// _Fct is of type void(int32_t _current, int32_t _parent) where the first argument
// is the child of the second argument. _parent is kNoParent if the _current
// joint is a root. _from indicates the joint from which the joint hierarchy
// traversal begins. Use Skeleton::kNoParent to traverse the whole
// hierarchy, in case there are multiple roots.
template <typename _Fct>
inline _Fct IterateJointsDF(const Skeleton& _skeleton, _Fct _fct,
                            int32_t _from = Skeleton::kNoParent) {
  const span<const int16_t>& parents = _skeleton.joint_parents();
  const int32_t num_joints = _skeleton.num_joints();
  //
  // parents[i] >= _from is true as long as "i" is a child of "_from".
  static_assert(Skeleton::kNoParent < 0,
                "Algorithm relies on kNoParent being negative");
  for (int32_t i = _from < 0 ? 0 : _from, process = i < num_joints; process;
       ++i, process = i < num_joints && parents[i] >= _from) {
    _fct(i, parents[i]);
  }
  return _fct;
}

// Applies a specified functor to each joint in a reverse (from leaves to root)
// depth-first order. _Fct is of type void(int32_t _current, int32_t _parent) where the
// first argument is the child of the second argument. _parent is kNoParent if
// the _current joint is a root.
template <typename _Fct>
inline _Fct IterateJointsDFReverse(const Skeleton& _skeleton, _Fct _fct) {
  const span<const int16_t>& parents = _skeleton.joint_parents();
  for (int32_t i = _skeleton.num_joints() - 1; i >= 0; --i) {
    _fct(i, parents[i]);
  }
  return _fct;
}
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_
