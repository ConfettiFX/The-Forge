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

#ifndef OZZ_OZZ_BASE_MATHS_MATH_CONSTANT_H_
#define OZZ_OZZ_BASE_MATHS_MATH_CONSTANT_H_

#ifndef INCLUDE_OZZ_MATH_CONSTANT_H_
#define INCLUDE_OZZ_MATH_CONSTANT_H_

namespace ozz {
namespace math {

// Defines math trigonometric constants.
static const float k2Pi = 6.283185307179586476925286766559f;
static const float kPi = 3.1415926535897932384626433832795f;
static const float kPi_2 = 1.5707963267948966192313216916398f;
static const float kPi_4 = .78539816339744830961566084581988f;
static const float kSqrt3 = 1.7320508075688772935274463415059f;
static const float kSqrt3_2 = 0.86602540378443864676372317075294f;
static const float kSqrt2 = 1.4142135623730950488016887242097f;
static const float kSqrt2_2 = 0.70710678118654752440084436210485f;

// Angle unit conversion constants.
static const float kDegreeToRadian = kPi / 180.f;
static const float kRadianToDegree = 180.f / kPi;

// Defines the square normalization tolerance value.
static const float kNormalizationToleranceSq = 1e-6f;
static const float kNormalizationToleranceEstSq = 2e-3f;

// Defines the square orthogonalisation tolerance value.
static const float kOrthogonalisationToleranceSq = 1e-16f;
}  // namespace math
}  // namespace ozz

#endif  // INCLUDE_OZZ_MATH_CONSTANT_H_
#endif  // OZZ_OZZ_BASE_MATHS_MATH_CONSTANT_H_
