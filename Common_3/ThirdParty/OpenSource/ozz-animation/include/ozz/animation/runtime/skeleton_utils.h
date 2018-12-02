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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_

#include "skeleton.h"

namespace ozz {
namespace animation {

// Get bind-pose of a skeleton joint.
AffineTransform GetJointLocalBindPose(const Skeleton& _skeleton, int _joint); //CONFFX_BEGIN

// Defines the iterator structure used by IterateJointsDF to traverse joint
// hierarchy.
struct JointsIterator {
  uint16_t joints[Skeleton::kMaxJoints];
  int num_joints;
};

// Fills _iterator with the index of the joints of _skeleton traversed in depth-
// first order.
// _from indicates the join from which the joint hierarchy traversal begins. Use
// Skeleton::kNoParentIndex to traverse the whole hierarchy, even if there are
// multiple roots.
// This function does not use a recursive implementation, to enforce a
// predictable stack usage, independent off the data (joint hierarchy) being
// processed.
void IterateJointsDF(const Skeleton& _skeleton, int _from,
                     JointsIterator* _iterator);

// Applies a specified functor to each joint in a depth-first order.
// _Fct is of type void(int _current, int _parent) where the first argument is
// the child of the second argument. _parent is kNoParentIndex if the _current
// joint is the root.
// _from indicates the join from which the joint hierarchy traversal begins. Use
// Skeleton::kNoParentIndex to traverse the whole hierarchy, even if there are
// multiple joints.
// This implementation is based on IterateJointsDF(*, *, JointsIterator$)
// variant.
template <typename _Fct>
inline _Fct IterateJointsDF(const Skeleton& _skeleton, int _from, _Fct _fct) {
  // Iterates and fills iterator.
  JointsIterator iterator;
  IterateJointsDF(_skeleton, _from, &iterator);

  // Consumes iterator and call _fct.
  Range<const Skeleton::JointProperties> properties =
      _skeleton.joint_properties();
  for (int i = 0; i < iterator.num_joints; ++i) {
    const int joint = iterator.joints[i];
    _fct(joint, properties.begin[joint].parent);
  }
  return _fct;
}
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_SKELETON_UTILS_H_
