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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_LOCAL_TO_MODEL_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_LOCAL_TO_MODEL_JOB_H_

#include "../../../../include/ozz/animation/runtime/export.h"
#include "../../../../include/ozz/base/platform.h"
#include "../../../../include/ozz/base/span.h"

namespace ozz {

namespace animation {

// Forward declares the Skeleton object used to describe joint hierarchy.
class Skeleton;

// Computes model-space joint matrices from local-space SoaTransform.
// This job uses the skeleton to define joints parent-child hierarchy. The job
// iterates through all joints to compute their transform relatively to the
// skeleton root.
// Job inputs is an array of SoaTransform objects (in local-space), ordered like
// skeleton's joints. Job output is an array of matrices (in model-space),
// ordered like skeleton's joints. Output are matrices, because the combination
// of affine transformations can contain shearing or complex transformation
// that cannot be represented as Transform object.
struct OZZ_ANIMATION_DLL LocalToModelJob {
  // Default constructor, initializes default values.
  LocalToModelJob();

  // Validates job parameters. Returns true for a valid job, or false otherwise:
  // -if any input pointer, including ranges, is NULL.
  // -if the size of the input is smaller than the skeleton's number of joints.
  // Note that this input has a SoA format.
  // -if the size of of the output is smaller than the skeleton's number of
  // joints.
  bool Validate() const;

  // Runs job's local-to-model task.
  // The job is validated before any operation is performed, see Validate() for
  // more details.
  // Returns false if job is not valid. See Validate() function.
  bool Run() const;

  // Job input.

  // The Skeleton object describing the joint hierarchy used for local to
  // model space conversion.
  const Skeleton* skeleton;

  // The root matrix will multiply to every model space matrices, default NULL
  // means an identity matrix. This can be used to directly compute world-space
  // transforms for example.
  const Matrix4* root;

  // Defines "from" which joint the local-to-model conversion should start.
  // Default value is ozz::Skeleton::kNoParent, meaning the whole hierarchy is
  // updated. This parameter can be used to optimize update by limiting
  // conversion to part of the joint hierarchy. Note that "from" parent should
  // be a valid matrix, as it is going to be used as part of "from" joint
  // hierarchy update.
  int32_t from;

  // Defines "to" which joint the local-to-model conversion should go, "to"
  // included. Update will end before "to" joint is reached if "to" is not part
  // of the hierarchy starting from "from". Default value is
  // ozz::animation::Skeleton::kMaxJoints, meaning the hierarchy (starting from
  // "from") is updated to the last joint.
  int32_t to;

  // If true, "from" joint is not updated during job execution. Update starts
  // with all children of "from". This can be used to update a model-space
  // transform independently from the local-space one. To do so: set "from"
  // joint model-space transform matrix, and run this Job with "from_excluded"
  // to update all "from" children.
  // Default value is false.
  bool from_excluded;

  // The input range that store local transforms.
  span<const SoaTransform> input;

  // Job output.

  // The output range to be filled with model-space matrices.
  span<Matrix4> output;
};
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_LOCAL_TO_MODEL_JOB_H_
