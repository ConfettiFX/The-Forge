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

#ifndef OZZ_ANIMATION_OFFLINE_DECIMATE_H_
#define OZZ_ANIMATION_OFFLINE_DECIMATE_H_

#ifndef OZZ_INCLUDE_PRIVATE_HEADER
#error "This header is private, it cannot be included from public headers."
#endif  // OZZ_INCLUDE_PRIVATE_HEADER

#include "../../../include/ozz/base/platform.h"
#include "../../../include/ozz/base/containers/vector.h"


namespace ozz {
namespace animation {
namespace offline {

// Decimation algorithm based on Ramer–Douglas–Peucker.
// https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm
// _Track must have std::vector interface.
// Adapter must have the following interface:
// struct Adapter {
//  bool Decimable(const Key&) const;
//  Key Lerp(const Key& _left, const Key& _right, const Key& _ref) const;
//  float Distance(const Key& _a, const Key& _b) const;
// };
template <typename _Track, typename _Adapter>
void Decimate(const _Track& _src, const _Adapter& _adapter, float _tolerance,
              _Track* _dest) {
  // Early out if not enough data.
  if ((uint32_t)arrlen(_src) < 2) {
    *_dest = _src;
    return;
  }

  // Stack of segments to process.
  typedef std::pair<size_t, size_t> Segment;
  Segment* segments = nullptr;

  // Bit vector of all points to included.
  bool* included = nullptr;
  arrsetlen(included, (uint32_t)arrlen(_src));
  memset(included, 0, sizeof(bool) * arrlen(included));

  // Pushes segment made from first and last points.
  arrpush(segments, Segment(0, (uint32_t)arrlen(_src) - 1));
  included[0] = true;
  included[(uint32_t)arrlen(_src) - 1] = true;

  // Empties segments stack.
  while (arrlen(segments) != 0) {
    // Pops next segment to process.
    const Segment segment = arrpop(segments);

    // Looks for the furthest point from the segment.
    float max = -1.f;
    size_t candidate = segment.first;
    const auto& left = _src[segment.first];
    const auto& right = _src[segment.second];
    for (size_t i = segment.first + 1; i < segment.second; ++i) {
      ASSERT(!included[i] && "Included points should be processed once only.");
      const auto& test = _src[i];
      if (!_adapter.Decimable(test)) {
        candidate = i;
        break;
      } else {
        const float distance =
            _adapter.Distance(_adapter.Lerp(left, right, test), test);
        if (distance > _tolerance && distance > max) {
          max = distance;
          candidate = i;
        }
      }
    }

    // If found, include the point and pushes the 2 new segments (before and
    // after the new point).
    if (candidate != segment.first) {
      included[candidate] = true;
      if (candidate - segment.first > 1) {
        arrpush(segments, Segment(segment.first, candidate));
      }
      if (segment.second - candidate > 1) {
        arrpush(segments, Segment(candidate, segment.second));
      }
    }
  }

  // Copy all included points.
  arrsetcap(*_dest, 0);
  for (size_t i = 0; i < (uint32_t)arrlen(_src); ++i) {
    if (included[i]) {
      arrpush(*_dest, _src[i]);
    }
  }

  // Removes last key if constant.
  if (arrlen(*_dest) > 1) {
    _Track end = &arrlast(*_dest);
    const auto& last = *(--end);
    const auto& penultimate = *(--end);
    const float distance = _adapter.Distance(penultimate, last);
    if (_adapter.Decimable(last) && distance <= _tolerance) {
      (void)arrpop(*_dest);
    }
  }

  arrfree(included);
  arrfree(segments);
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_ANIMATION_OFFLINE_DECIMATE_H_
