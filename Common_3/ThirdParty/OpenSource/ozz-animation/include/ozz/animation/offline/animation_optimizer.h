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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_ANIMATION_OPTIMIZER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_ANIMATION_OPTIMIZER_H_

namespace ozz {
namespace animation {

// Forward declare runtime skeleton type.
class Skeleton;
namespace offline {

// Forward declare offline animation type.
struct RawAnimation;

// Defines the class responsible of optimizing an offline raw animation
// instance. Optimization is performed using a key frame reduction technique. It
// strips redundant / interpolable key frames, within error tolerances given as
// input.
// The optimizer also takes into account for each joint the error generated on
// its whole child hierarchy, with the hierarchical tolerance value. This allows
// for example to take into consideration the error generated on a finger when
// optimizing the shoulder. A small error on the shoulder can be magnified when
// propagated to the finger indeed.
// Default optimization tolerances are set in order to favor quality
// over runtime performances and memory footprint.
class AnimationOptimizer {
 public:
  // Initializes the optimizer with default tolerances (favoring quality).
  AnimationOptimizer();

  // Optimizes _input using *this parameters. _skeleton is required to evaluate
  // optimization error along joint hierarchy (see hierarchical_tolerance).
  // Returns true on success and fills _output animation with the optimized
  // version of _input animation.
  // *_output must be a valid RawAnimation instance.
  // Returns false on failure and resets _output to an empty animation.
  // See RawAnimation::Validate() for more details about failure reasons.
  bool operator()(const RawAnimation& _input, const Skeleton& _skeleton,
                  RawAnimation* _output) const;

  // Translation optimization tolerance, defined as the distance between two
  // translation values in meters.
  float translation_tolerance;

  // Rotation optimization tolerance, ie: the angle between two rotation values
  // in radian.
  float rotation_tolerance;

  // Scale optimization tolerance, ie: the norm of the difference of two scales.
  float scale_tolerance;

  // Hierarchical translation optimization tolerance, ie: the maximum error
  // (distance) that an optimization on a joint is allowed to generate on its
  // whole child hierarchy.
  float hierarchical_tolerance;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_ANIMATION_OPTIMIZER_H_
