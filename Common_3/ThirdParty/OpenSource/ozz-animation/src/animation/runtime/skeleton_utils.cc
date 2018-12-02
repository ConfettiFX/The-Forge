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
      _skeleton.bind_pose()[_joint / 4];

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
//CONFFX_END

// Helper macro used to detect if a joint has a brother.
#define _HAS_SIBLING(_i, _num_joints, _properties) \
  ((_i + 1 < _num_joints) &&                       \
   (_properties[_i].parent == _properties[_i + 1].parent))

// Implement joint hierarchy depth-first traversal.
// Uses a non-recursive implementation to control stack usage (ie: making
// algorithm behavior (stack consumption) independent off the data being
// processed).
void IterateJointsDF(const Skeleton& _skeleton, int _from,
                     JointsIterator* _iterator) {
  assert(_iterator);
  const int num_joints = _skeleton.num_joints();
  Range<const Skeleton::JointProperties> properties =
      _skeleton.joint_properties();

  // Initialize iterator.
  _iterator->num_joints = 0;

  // Validates input range first.
  if (num_joints == 0) {
    return;
  }
  if ((_from < 0 || _from >= num_joints) && _from != Skeleton::kNoParentIndex) {
    return;
  }

  // Simulate a stack to unroll usual recursive implementation.
  struct Context {
    uint16_t joint : 15;
    uint16_t has_brother : 1;
  };
  Context stack[Skeleton::kMaxJoints];
  int stack_size = 0;

  // Initializes iteration start.
  Context start;
  if (_from != Skeleton::kNoParentIndex) {
    start.joint = _from;
    start.has_brother = false;  // Disallow brother processing.
  } else {  // num_joints > 0, which was tested as pre-conditions.
    start.joint = 0;
    start.has_brother = _HAS_SIBLING(0, num_joints, properties.begin);
  }
  stack[stack_size++] = start;

  for (; stack_size != 0;) {
    // Process next joint on the stack.
    const Context& top = stack[stack_size - 1];

    // Push that joint to the list and then process its child.
    _iterator->joints[_iterator->num_joints++] = top.joint;

    // Skip all the joints until the first child is found.
    if (!properties.begin[top.joint].is_leaf) {  // A leaf has no child anyway.
      uint16_t next_joint = top.joint + 1;
      for (; next_joint < num_joints &&
             top.joint != properties.begin[next_joint].parent;
           ++next_joint) {
      }
      if (next_joint < num_joints) {
        Context& next = stack[stack_size++];  // Push child and process it.
        next.joint = next_joint;
        next.has_brother =
            _HAS_SIBLING(next_joint, num_joints, properties.begin);
        continue;
      }
    }

    // Rewind the stack while there's no brother to process.
    for (; stack_size != 0 && !stack[stack_size - 1].has_brother;
         --stack_size) {
    }

    // Replace top joint by its brother.
    if (stack_size != 0) {
      Context& next = stack[stack_size - 1];
      assert(next.has_brother && next.joint + 1 < num_joints);

      ++next.joint;  // The brother is the next joint in breadth-first order.
      next.has_brother = _HAS_SIBLING(next.joint, num_joints, properties.begin);
    }
  }
}
#undef _HAS_SIBLING
}  // namespace animation
}  // namespace ozz
//CONFFX_END
