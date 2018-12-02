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

#include "ozz/animation/runtime/track_triggering_job.h"
#include "ozz/animation/runtime/track.h"

#include <algorithm>
#include <cassert>

namespace ozz {
namespace animation {

TrackTriggeringJob::TrackTriggeringJob()
    : from(0.f), to(0.f), threshold(0.f), track(NULL), iterator(NULL) {}

bool TrackTriggeringJob::Validate() const {
  bool valid = true;
  valid &= track != NULL;
  valid &= iterator != NULL;
  return valid;
}

bool TrackTriggeringJob::Run() const {
  if (!Validate()) {
    return false;
  }

  // Triggering can only happen in a valid range of ratio.
  if (from == to) {
    *iterator = end();
    return true;
  }

  *iterator = Iterator(this);

  return true;
}

namespace {
inline bool DetectEdge(ptrdiff_t _i0, ptrdiff_t _i1, bool _forward,
                       const TrackTriggeringJob& _job,
                       TrackTriggeringJob::Edge* _edge) {
  const Range<const float>& values = _job.track->values();

  const float vk0 = values[_i0];
  const float vk1 = values[_i1];

  bool detected = false;
  if (vk0 <= _job.threshold && vk1 > _job.threshold) {
    // Rising edge
    _edge->rising = _forward;
    detected = true;
  } else if (vk0 > _job.threshold && vk1 <= _job.threshold) {
    // Falling edge
    _edge->rising = !_forward;
    detected = true;
  }

  if (detected) {
    const Range<const float>& ratios = _job.track->ratios();
    const Range<const uint8_t>& steps = _job.track->steps();

    const bool step = (steps[_i0 / 8] & (1 << (_i0 & 7))) != 0;
    if (step) {
      _edge->ratio = ratios[_i1];
    } else {
      assert(vk0 != vk1);  // Won't divide by 0

      if (_i1 == 0) {
        _edge->ratio = 0.f;
      } else {
        // Finds where the curve crosses threshold value.
        // This is the lerp equation, where we know the result and look for
        // alpha, aka un-lerp.
        const float alpha = (_job.threshold - vk0) / (vk1 - vk0);

        // Remaps to keyframes actual times.
        const float tk0 = ratios[_i0];
        const float tk1 = ratios[_i1];
        _edge->ratio = lerp(tk0, tk1, alpha); //CONFFX_BEGIN
      }
    }
  }
  return detected;
}
}  // namespace

TrackTriggeringJob::Iterator::Iterator(const TrackTriggeringJob* _job)
    : job_(_job) {
  // Outer loop initialization.
  outer_ = floorf(job_->from);

  // Search could start more closely to the "from" ratio, but it's not possible
  // to ensure that floating point precision will not lead to missing a key
  // (when from/to range is far from 0). This is less good in algorithmic
  // complexity, but for consistency of forward and backward triggering, it's
  // better to let iterator ++ implementation filter included and excluded
  // edges.
  inner_ = job_->from < job_->to ? 0 : _job->track->ratios().count() - 1;

  // Evaluates first edge.
  ++*this;
}

const TrackTriggeringJob::Iterator& TrackTriggeringJob::Iterator::operator++() {
  assert(*this != job_->end() && "Can't increment end iterator.");

  const Range<const float>& ratios = job_->track->ratios();
  const ptrdiff_t num_keys = ratios.count();

  if (job_->to > job_->from) {
    for (; outer_ < job_->to; outer_ += 1.f) {
      for (; inner_ < num_keys; ++inner_) {
        const ptrdiff_t i0 = inner_ == 0 ? num_keys - 1 : inner_ - 1;
        if (DetectEdge(i0, inner_, true, *job_, &edge_)) {
          edge_.ratio += outer_;  // Convert to global ratio space.
          if (edge_.ratio >= job_->from &&
              (edge_.ratio < job_->to || job_->to >= 1.f + outer_)) {
            ++inner_;
            return *this;  // Yield found edge.
          }
          // Won't find any further edge.
          if (ratios[inner_] + outer_ >= job_->to) {
            break;
          }
        }
      }
      inner_ = 0;  // Ready for next loop.
    }
  } else {
    for (; outer_ + 1.f > job_->to; outer_ -= 1.f) {
      for (; inner_ >= 0; --inner_) {
        const ptrdiff_t i0 = inner_ == 0 ? num_keys - 1 : inner_ - 1;
        if (DetectEdge(i0, inner_, false, *job_, &edge_)) {
          edge_.ratio += outer_;  // Convert to global ratio space.
          if (edge_.ratio >= job_->to &&
              (edge_.ratio < job_->from || job_->from >= 1.f + outer_)) {
            --inner_;
            return *this;  // Yield found edge.
          }
        }
        // Won't find any further edge.
        if (ratios[inner_] + outer_ <= job_->to) {
          break;
        }
      }
      inner_ = ratios.count() - 1;  // Ready for next loop.
    }
  }

  // Set iterator to end position.
  *this = job_->end();

  return *this;
}
}  // namespace animation
}  // namespace ozz
