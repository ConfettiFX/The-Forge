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

#include "../../../include/ozz/animation/runtime/local_to_model_job.h"



#include "../../../include/ozz/base/maths/math_ex.h"

#include "../../../include/ozz/animation/runtime/skeleton.h"

namespace ozz {
namespace animation {

LocalToModelJob::LocalToModelJob()
    : skeleton(NULL),
      root(NULL),
      from(Skeleton::kNoParent),
      to(Skeleton::kMaxJoints),
      from_excluded(false) {}

bool LocalToModelJob::Validate() const {
  // Don't need any early out, as jobs are valid in most of the performance
  // critical cases.
  // Tests are written in multiple lines in order to avoid branches.
  bool valid = true;

  // Test for NULL begin pointers.
  if (!skeleton) {
    return false;
  }

  const size_t num_joints = static_cast<size_t>(skeleton->num_joints());
  const size_t num_soa_joints = (num_joints + 3) / 4;

  // Test input and output ranges, implicitly tests for NULL end pointers.
  valid &= input.size() >= num_soa_joints;
  valid &= output.size() >= num_joints;

  return valid;
}

bool LocalToModelJob::Run() const {
  if (!Validate()) {
    return false;
  }

  const span<const int16_t>& parents = skeleton->joint_parents();

  // Initializes an identity matrix that will be used to compute roots model
  // matrices without requiring a branch.
  const Matrix4 identity = Matrix4::identity();
  const Matrix4* root_matrix = (root == NULL) ? &identity : root;

  // Applies hierarchical transformation.
  // Loop ends after "to".
  const int32_t end = math::Min(to + 1, skeleton->num_joints());
  // Begins iteration from "from", or the next joint if "from" is excluded.
  // Process next joint if end is not reach. parents[begin] >= from is true as
  // long as "begin" is a child of "from".
  for (int32_t i = math::Max(from + from_excluded, 0),
           process = i < end && (!from_excluded || parents[i] >= from);
       process;) {
    // Builds soa matrices from soa transforms.
    const SoaTransform& transform = input[i / 4];
    const SoaFloat4x4 local_soa_matrices = SoaFloat4x4::FromAffine(
        transform.translation, transform.rotation, transform.scale);

    // Converts to aos matrices.
    Vector4 local_aos_matrices[16];
    transpose16x16(&local_soa_matrices.cols[0].x, local_aos_matrices);

    // parents[i] >= from is true as long as "i" is a child of "from".
    const Vector4* local_aos_matrix = local_aos_matrices;
    for (const int32_t soa_end = (i + 4) & ~3; i < soa_end && process;
        local_aos_matrix += 4, ++i, process = i < end && parents[i] >= from) {
      const int32_t parent = parents[i];

      const Matrix4 local_matrix =
          Matrix4(local_aos_matrix[0], local_aos_matrix[1], local_aos_matrix[2],
                  local_aos_matrix[3]);
      const Matrix4* parent_matrix =
          parent == Skeleton::kNoParent ? root_matrix : &output[parent];
      output[i] = (*parent_matrix) * local_matrix;
    }
  }
  return true;
}
}  // namespace animation
}  // namespace ozz
