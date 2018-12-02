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

#include "ozz/animation/offline/skeleton_builder.h"

#include <cstring>

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/vector.h"
//#include "ozz/base/maths/soa_transform.h" //CONFFX_BEGIN
#include "ozz/base/memory/allocator.h"

namespace ozz {
namespace animation {
namespace offline {

namespace {
// Stores each traversed joint to a vector.
struct JointLister {
  explicit JointLister(int _num_joints) { linear_joints.reserve(_num_joints); }
  void operator()(const RawSkeleton::Joint& _current,
                  const RawSkeleton::Joint* _parent) {
    // Looks for the "lister" parent.
    int parent = Skeleton::kNoParentIndex;
    if (_parent) {
      // Start searching from the last joint.
      int j = static_cast<int>(linear_joints.size()) - 1;
      for (; j >= 0; --j) {
        if (linear_joints[j].joint == _parent) {
          parent = j;
          break;
        }
      }
      assert(parent >= 0);
    }
    const Joint listed = {&_current, parent};
    linear_joints.push_back(listed);
  }
  struct Joint {
    const RawSkeleton::Joint* joint;
    int parent;
  };
  // Array of joints in the traversed DAG order.
  ozz::Vector<Joint>::Std linear_joints;
};
}  // namespace

// Validates the RawSkeleton and fills a Skeleton.
// Uses RawSkeleton::IterateJointsBF to traverse in DAG breadth-first order.
// This favors cache coherency (when traversing joints) and reduces
// Load-Hit-Stores (reusing the parent that has just been computed).
/*Skeleton* SkeletonBuilder::operator()(const RawSkeleton& _raw_skeleton) const {
  // Tests _raw_skeleton validity.
  if (!_raw_skeleton.Validate()) {
    return NULL;
  }

  // Everything is fine, allocates and fills the skeleton.
  // Will not fail.
  Skeleton* skeleton = memory::default_allocator()->New<Skeleton>();
  const int num_joints = _raw_skeleton.num_joints();

  // Iterates through all the joint of the raw skeleton and fills a sorted joint
  // list.
  JointLister lister(num_joints);
  _raw_skeleton.IterateJointsBF<JointLister&>(lister);
  assert(static_cast<int>(lister.linear_joints.size()) == num_joints);

  // Computes name's buffer size.
  size_t chars_size = 0;
  for (int i = 0; i < num_joints; ++i) {
    const RawSkeleton::Joint& current = *lister.linear_joints[i].joint;
    chars_size += (current.name.size() + 1) * sizeof(char);
  }

  // Allocates all skeleton members.
  char* cursor = skeleton->Allocate(chars_size, num_joints);

  // Copy names. All names are allocated in a single buffer. Only the first name
  // is set, all other names array entries must be initialized.
  for (int i = 0; i < num_joints; ++i) {
    const RawSkeleton::Joint& current = *lister.linear_joints[i].joint;
    skeleton->joint_names_[i] = cursor;
    strcpy(cursor, current.name.c_str());
    cursor += (current.name.size() + 1) * sizeof(char);
  }

  // Transfers sorted joints hierarchy to the new skeleton.
  for (int i = 0; i < num_joints; ++i) {
    skeleton->joint_properties_[i].parent = lister.linear_joints[i].parent;
    skeleton->joint_properties_[i].is_leaf =
        lister.linear_joints[i].joint->children.empty();
  }

  // Transfers t-poses.
  //CONFFX_BEGIN
  const Vector4 w_axis = Vector4::wAxis();
  const Vector4 zero = Vector4::zero();
  const Vector4 one = Vector4::one();

  for (int i = 0; i < skeleton->num_soa_joints(); ++i) {
    Vector4 translations[4];
    Vector4 scales[4];
    Vector4 rotations[4];
    for (int j = 0; j < 4; ++j) {
      if (i * 4 + j < num_joints) {
        const RawSkeleton::Joint& src_joint =
            *lister.linear_joints[i * 4 + j].joint;
        translations[j] =
            Vector4(src_joint.transform.translation, 0.f);

		if (norm(src_joint.transform.rotation) != 0.f)
		{
          rotations[j] = Vector4(normalize(src_joint.transform.rotation));
		}
		else
		{
          rotations[j] = w_axis;
		}

        scales[j] = Vector4(src_joint.transform.scale, 0.f);
      } else {
        translations[j] = zero;
        rotations[j] = w_axis;
        scales[j] = one;
      }
    }
    // Fills the SoaTransform structure.
    transpose4x3(translations, &skeleton->bind_pose_[i].translation.x);
    transpose4x4(rotations, &skeleton->bind_pose_[i].rotation.x);
    transpose4x3(scales, &skeleton->bind_pose_[i].scale.x);
  }

  return skeleton;  // Success.
}*/
//CONFFX_END

//CONFFX_BEGIN
bool SkeletonBuilder::Build(const RawSkeleton& _raw_skeleton, Skeleton* skeleton)
{
	// Tests _raw_skeleton validity.
	if (!_raw_skeleton.Validate())
	{
		return false;
	}

	const int num_joints = _raw_skeleton.num_joints();

	// Iterates through all the joint of the raw skeleton and fills a sorted joint
	// list.
	JointLister lister(num_joints);
	_raw_skeleton.IterateJointsBF<JointLister&>(lister);
	assert(static_cast<int>(lister.linear_joints.size()) == num_joints);

	// Computes name's buffer size.
	size_t chars_size = 0;
	for (int i = 0; i < num_joints; ++i)
	{
		const RawSkeleton::Joint& current = *lister.linear_joints[i].joint;
		chars_size += (current.name.size() + 1) * sizeof(char);
	}

	// Allocates all skeleton members.
	char* cursor = skeleton->Allocate(chars_size, num_joints);

	// Copy names. All names are allocated in a single buffer. Only the first name
	// is set, all other names array entries must be initialized.
	for (int i = 0; i < num_joints; ++i)
	{
		const RawSkeleton::Joint& current = *lister.linear_joints[i].joint;
		skeleton->joint_names_[i] = cursor;
		strcpy(cursor, current.name.c_str());
		cursor += (current.name.size() + 1) * sizeof(char);
	}

	// Transfers sorted joints hierarchy to the new skeleton.
	for (int i = 0; i < num_joints; ++i)
	{
		skeleton->joint_properties_[i].parent = lister.linear_joints[i].parent;
		skeleton->joint_properties_[i].is_leaf =
			lister.linear_joints[i].joint->children.empty();
	}

	// Transfers t-poses.
	//CONFFX_BEGIN
	const Vector4 w_axis = Vector4::wAxis();
	const Vector4 zero = Vector4::zero();
	const Vector4 one = Vector4::one();

	for (int i = 0; i < skeleton->num_soa_joints(); ++i)
	{
		Vector4 translations[4];
		Vector4 scales[4];
		Vector4 rotations[4];
		for (int j = 0; j < 4; ++j)
		{
			if (i * 4 + j < num_joints)
			{
				const RawSkeleton::Joint& src_joint =
					*lister.linear_joints[i * 4 + j].joint;
				translations[j] =
					Vector4(src_joint.transform.translation, 0.f);

				if (norm(src_joint.transform.rotation) != 0.f)
				{
					rotations[j] = Vector4(normalize(src_joint.transform.rotation));
				}
				else
				{
					rotations[j] = w_axis;
				}

				scales[j] = Vector4(src_joint.transform.scale, 0.f);
			}
			else
			{
				translations[j] = zero;
				rotations[j] = w_axis;
				scales[j] = one;
			}
		}
		// Fills the SoaTransform structure.
		transpose4x3(translations, &skeleton->bind_pose_[i].translation.x);
		transpose4x4(rotations, &skeleton->bind_pose_[i].rotation.x);
		transpose4x3(scales, &skeleton->bind_pose_[i].scale.x);
	}

	return true;  // Success.
}
// CONFFX_END
}  // namespace offline
}  // namespace animation
}  // namespace ozz
