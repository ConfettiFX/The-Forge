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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_LOCAL_TO_MODEL_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_LOCAL_TO_MODEL_JOB_H_

//CONFFX_BEGIN
#include "../../base/platform.h"
#include "../../../../../../../OS/Math/MathTypes.h"

namespace ozz {

//CONFFX_END

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
struct LocalToModelJob {
  // Default constructor, initializes default values.
  LocalToModelJob() : skeleton(NULL), root(NULL) {}

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

  // The Skeleton object describing the joint hierarchy used for local to
  // model space conversion.
  const Skeleton* skeleton;

  // The root matrix will multiply to every model space matrices, default NULL
  // means an identity matrix.
  const Matrix4* root; //CONFFX_BEGIN

  // Job input.
  // The input range that store local transforms.
  Range<const SoaTransform> input; //CONFFX_BEGIN

  // Job output.
  // The output range to be filled with model matrices.
  Range<Matrix4> output; //CONFFX_BEGIN
};
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_LOCAL_TO_MODEL_JOB_H_
