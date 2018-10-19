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

#ifndef OZZ_OZZ_BASE_MATHS_TRANSFORM_H_
#define OZZ_OZZ_BASE_MATHS_TRANSFORM_H_

#include "quaternion.h"
#include "vec_float.h"
#include "../platform.h"

namespace ozz {
namespace math {

// Stores an affine transformation with separate translation, rotation and scale
// attributes.
struct Transform {
  // Translation affine transformation component.
  Float3 translation;

  // Rotation affine transformation component.
  Quaternion rotation;

  // Scale affine transformation component.
  Float3 scale;

  // Builds an identity transform.
  static OZZ_INLINE Transform identity() {
    const Transform ret = {Float3::zero(), Quaternion::identity(),
                           Float3::one()};
    return ret;
  }
};
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_TRANSFORM_H_
