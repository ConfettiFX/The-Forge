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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_ANIMATION_UTILS_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_ANIMATION_UTILS_H_

#include "../../../../include/ozz/animation/runtime/export.h"
#include "../../../../include/ozz/animation/runtime/animation.h"

namespace ozz {
namespace animation {

// Count translation, rotation or scale keyframes for a given track number. Use
// a negative _track value to count all tracks.
OZZ_ANIMATION_DLL int32_t CountTranslationKeyframes(const Animation& _animation,
                                                int32_t _track = -1);
OZZ_ANIMATION_DLL int32_t CountRotationKeyframes(const Animation& _animation,
                                             int32_t _track = -1);
OZZ_ANIMATION_DLL int32_t CountScaleKeyframes(const Animation& _animation,
                                          int32_t _track = -1);
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_ANIMATION_UTILS_H_
