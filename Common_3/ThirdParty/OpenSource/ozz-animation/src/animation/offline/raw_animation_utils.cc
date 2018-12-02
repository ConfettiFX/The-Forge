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

#include "ozz/animation/offline/raw_animation_utils.h"

namespace ozz {
namespace animation {
namespace offline {

//CONFFX_BEGIN
// Translation interpolation method.
// This must be the same Lerp as the one used by the sampling job.
Vector3 LerpTranslation(const Vector3& _a, const Vector3& _b,
                             float _alpha) {
  return lerp(_a, _b, _alpha);
}

// Rotation interpolation method.
// This must be the same Lerp as the one used by the sampling job.
// The goal is to take the shortest path between _a and _b. This code replicates
// this behavior that is actually not done at runtime, but when building the
// animation.
Quat LerpRotation(const Quat& _a,
                              const Quat& _b, float _alpha) {
  // Finds the shortest path. This is done by the AnimationBuilder for runtime
  // animations.
  const float dot = _a.getX() * _b.getX() + _a.getY() * _b.getY() + _a.getZ() * _b.getZ() + _a.getW() * _b.getW();
  return normalize(lerp(_alpha, _a, dot < 0.f ? -_b : _b));  // _b an -_b are the
                                                         // same rotation.
}

// Scale interpolation method.
// This must be the same Lerp as the one used by the sampling job.
Vector3 LerpScale(const Vector3& _a, const Vector3& _b,
                       float _alpha) {
  return lerp(_a, _b, _alpha);
}
//CONFFX_END
}  // namespace offline
}  // namespace animation
}  // namespace ozz
