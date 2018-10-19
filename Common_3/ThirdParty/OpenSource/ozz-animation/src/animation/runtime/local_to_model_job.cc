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
#include "../../../include/ozz/animation/runtime/local_to_model_job.h"
#include "../../../include/ozz/animation/runtime/skeleton.h"
#include "../../../include/ozz/base/maths/math_ex.h"
#include <cassert>
#include "../../../../../../OS/Math/MathTypes.h"
//CONFFX_END

namespace ozz {
namespace animation {

bool LocalToModelJob::Validate() const {
  // Don't need any early out, as jobs are valid in most of the performance
  // critical cases.
  // Tests are written in multiple lines in order to avoid branches.
  bool valid = true;

  // Test for NULL begin pointers.
  if (!skeleton) {
    return false;
  }
  valid &= input.begin != NULL;
  valid &= output.begin != NULL;

  const int num_joints = skeleton->num_joints();
  const int num_soa_joints = (num_joints + 3) / 4;

  // Test input and output ranges, implicitly tests for NULL end pointers.
  valid &= input.end - input.begin >= num_soa_joints;
  valid &= output.end - output.begin >= num_joints;

  return valid;
}

//CONFFX_BEGIN
bool LocalToModelJob::Run() const {

  if (!Validate()) {
    return false;
  }

  // Early out if no joint.
  const int num_joints = skeleton->num_joints();
  if (num_joints == 0) {
    return true;
  }

  // Fetch joint's properties.
  Range<const Skeleton::JointProperties> properties =
      skeleton->joint_properties();

  // Output.
  Matrix4* const model_matrices = output.begin;

  // Initializes an identity matrix that will be used to compute roots model
  // matrices without requiring a branch.
  const Matrix4 identity = Matrix4::identity();
  const Matrix4* root_matrix = (root == NULL) ? &identity : root;

  for (int joint = 0; joint < num_joints;) {
    // Builds soa matrices from soa transforms.
    const SoaTransform& transform = input.begin[joint / 4];
    const SoaFloat4x4 local_soa_matrices = SoaFloat4x4::FromAffine(
        transform.translation, transform.rotation, transform.scale);
    // Converts to aos matrices.
    Vector4 local_aos_matrices[16];
    transpose16x16(&local_soa_matrices.cols[0].x, local_aos_matrices);

    // Applies hierarchical transformation.
    const int proceed_up_to = joint + min(4, num_joints - joint);
    const Vector4* local_aos_matrix = local_aos_matrices;
    for (; joint < proceed_up_to; ++joint, local_aos_matrix += 4) {
      const int parent = properties.begin[joint].parent;
      const Matrix4* parent_matrix =
          math::Select(parent == Skeleton::kNoParentIndex, root_matrix,
                       &model_matrices[parent]);
      const Matrix4 local_matrix = Matrix4(local_aos_matrix[0], local_aos_matrix[1],
                                      local_aos_matrix[2],
                                      local_aos_matrix[3]);
      model_matrices[joint] = (*parent_matrix) * local_matrix;
    }
  }

  return true;
}
//CONFFX_END
}  // namespace animation
}  // namespace ozz
