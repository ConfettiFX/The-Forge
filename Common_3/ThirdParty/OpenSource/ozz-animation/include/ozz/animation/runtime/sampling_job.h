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

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_SAMPLING_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_SAMPLING_JOB_H_

//CONFFX_BEGIN
#include "../../base/platform.h"
#include "../../base/span.h"
#include "../../../../../../../../Common_3/OS/Math/MathTypes.h"

namespace ozz {

//CONFFX_END

namespace animation {

// Forward declares the animation type to sample.
class Animation;

// Forward declares the cache object used by the SamplingJob.
class SamplingCache;

// Samples an animation at a given time ratio in the unit interval [0,1] (where
// 0 is the beginning of the animation, 1 is the end), to output the
// corresponding posture in local-space.
// SamplingJob uses a cache (aka SamplingCache) to store intermediate values
// (decompressed animation keyframes...) while sampling. This cache also stores
// pre-computed values that allows drastic optimization while playing/sampling
// the animation forward. Backward sampling works, but isn't optimized through
// the cache. The job does not owned the buffers (in/output) and will thus not
// delete them during job's destruction.
struct SamplingJob {
  // Default constructor, initializes default values.
  SamplingJob();

  // Validates job parameters. Returns true for a valid job, or false otherwise:
  // -if any input pointer is nullptr
  // -if output range is invalid.
  bool Validate() const;

  // Runs job's sampling task.
  // The job is validated before any operation is performed, see Validate() for
  // more details.
  // Returns false if *this job is not valid.
  bool Run() const;

  // Time ratio in the unit interval [0,1] used to sample animation (where 0 is
  // the beginning of the animation, 1 is the end). It should be computed as the
  // current time in the animation , divided by animation duration.
  // This ratio is clamped before job execution in order to resolves any
  // approximation issue on range bounds.
  float ratio;

  // The animation to sample.
  const Animation* animation;

  // A cache object that must be big enough to sample *this animation.
  SamplingCache* cache;

  // Job output.
  // The output range to be filled with sampled joints during job execution.
  // If there are less joints in the animation compared to the output range,
  // then remaining SoaTransform are left unchanged.
  // If there are more joints in the animation, then the last joints are not
  // sampled.
  span<SoaTransform> output; //CONFFX_BEGIN
};

namespace internal {
// Soa hot data to interpolate.
struct InterpSoaFloat3;
struct InterpSoaQuaternion;
}  // namespace internal

// Declares the cache object used by the workload to take advantage of the
// frame coherency of animation sampling.
class SamplingCache {
 public:
  // Constructs an empty cache. The cache needs to be resized with the
  // appropriate number of tracks before it can be used with a SamplingJob.
  SamplingCache();

  // Constructs a cache that can be used to sample any animation with at most
  // _max_tracks tracks. _num_tracks is internally aligned to a multiple of
  // soa size, which means max_tracks() can return a different (but bigger)
  // value than _max_tracks.
  explicit SamplingCache(int _max_tracks);

  // Deallocates cache.
  ~SamplingCache();

  // Resize the number of joints that the cache can support.
  // This also implicitly invalidate the cache.
  void Resize(int _max_tracks);

  // Invalidate the cache.
  // The SamplingJob automatically invalidates a cache when required
  // during sampling. This automatic mechanism is based on the animation
  // address and sampling time ratio. The weak point is that it can result in a
  // crash if ever the address of an animation is used again with another
  // animation (could be the result of successive call to delete / new).
  // Therefore it is recommended to manually invalidate a cache when it is
  // known that this cache will not be used for with an animation again.
  void Invalidate();

  // The maximum number of tracks that the cache can handle.
  int max_tracks() const { return max_soa_tracks_ * 4; }
  int max_soa_tracks() const { return max_soa_tracks_; }

 private:
  // Disables copy and assignation.
  SamplingCache(SamplingCache const&);
  void operator=(SamplingCache const&);

  friend struct SamplingJob;

  // Steps the cache in order to use it for a potentially new animation and
  // ratio. If the _animation is different from the animation currently cached,
  // or if the _ratio shows that the animation is played backward, then the
  // cache is invalidated and reseted for the new _animation and _ratio.
  void Step(const Animation& _animation, float _ratio);

  // The animation this cache refers to. nullptr means that the cache is invalid.
  const Animation* animation_;

  // The current time ratio in the animation.
  float ratio_;

  // The number of soa tracks that can store this cache.
  int max_soa_tracks_;

  // Soa hot data to interpolate.
  internal::InterpSoaFloat3* soa_translations_;
  internal::InterpSoaQuaternion* soa_rotations_;
  internal::InterpSoaFloat3* soa_scales_;

  // Points to the keys in the animation that are valid for the current time
  // ratio.
  int* translation_keys_;
  int* rotation_keys_;
  int* scale_keys_;

  // Current cursors in the animation. 0 means that the cache is invalid.
  int translation_cursor_;
  int rotation_cursor_;
  int scale_cursor_;

  // Outdated soa entries. One bit per soa entry (32 joints per byte).
  uint8_t* outdated_translations_;
  uint8_t* outdated_rotations_;
  uint8_t* outdated_scales_;
};
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_SAMPLING_JOB_H_
