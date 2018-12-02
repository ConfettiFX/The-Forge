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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_SKELETON_BUILDER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_SKELETON_BUILDER_H_

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"

//CONFFX_BEGIN
#include "../../../../../../../OS/Math/MathTypes.h"
//CONFFX_END

namespace ozz {
namespace animation {

// Forward declares the runtime skeleton type.
class Skeleton;

namespace offline {

// Forward declares the offline skeleton type.
struct RawSkeleton;

// Defines the class responsible of building Skeleton instances.
class SkeletonBuilder {
 public:
  // Creates a Skeleton based on _raw_skeleton and *this builder parameters.
  // Returns a Skeleton instance on success which will then be deleted using
  // the default allocator Delete() function.
  // Returns NULL on failure. See RawSkeleton::Validate() for more details about
  // failure reasons.
  //Skeleton* operator()(const RawSkeleton& _raw_skeleton) const;	// Deleted because it causes problems with the memory allocator
	 static bool Build(const RawSkeleton& _raw_skeleton, Skeleton* skeleton);
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_SKELETON_BUILDER_H_
