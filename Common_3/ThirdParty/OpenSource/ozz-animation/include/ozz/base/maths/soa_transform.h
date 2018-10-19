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

#ifndef OZZ_OZZ_BASE_MATHS_SOA_TRANSFORM_H_
#define OZZ_OZZ_BASE_MATHS_SOA_TRANSFORM_H_

#include "soa_float.h"
#include "soa_quaternion.h"
#include "../platform.h"

namespace ozz {
namespace math {

// Stores an affine transformation with separate translation, rotation and scale
// attributes.
struct SoaTransform {
  SoaFloat3 translation;
  SoaQuaternion rotation;
  SoaFloat3 scale;

  static OZZ_INLINE SoaTransform identity() {
    const SoaTransform ret = {SoaFloat3::zero(), SoaQuaternion::identity(),
                              SoaFloat3::one()};
    return ret;
  }
};
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_SOA_TRANSFORM_H_
