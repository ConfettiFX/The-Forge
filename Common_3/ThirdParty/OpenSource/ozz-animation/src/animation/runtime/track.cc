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

#include "ozz/animation/runtime/track.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_archive.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/memory/allocator.h"

#include "../../EASTL/internal/char_traits.h"
#include "../../../../../Common_3/OS/Interfaces/ILog.h"

#include <cassert>

namespace ozz {
namespace animation {

namespace internal {

template <typename _ValueType>
Track<_ValueType>::Track() : name_(nullptr) {}

template <typename _ValueType>
Track<_ValueType>::~Track() {
  Deallocate();
}

template <typename _ValueType>
void Track<_ValueType>::Allocate(size_t _keys_count, size_t _name_len) {
  assert(ratios_.size() == 0 && values_.size() == 0);

  // Distributes buffer memory while ensuring proper alignment (serves larger
  // alignment values first).
  static_assert(alignof(_ValueType) >= alignof(float) &&
                    alignof(float) >= alignof(uint8_t),
                "Must serve larger alignment values first)");

  // Compute overall size and allocate a single buffer for all the data.
  const size_t buffer_size = _keys_count * sizeof(_ValueType) +  // values
                             _keys_count * sizeof(float) +       // ratios
                             (_keys_count + 7) * sizeof(uint8_t) / 8 +  // steps
                             (_name_len > 0 ? _name_len + 1 : 0);
  span<char> buffer = {static_cast<char*>(memory::default_allocator()->Allocate(
                           buffer_size, alignof(_ValueType))),
                       buffer_size};

  // Fix up pointers. Serves larger alignment values first.
  values_ = fill_span<_ValueType>(buffer, _keys_count);
  ratios_ = fill_span<float>(buffer, _keys_count);
  steps_ = fill_span<uint8_t>(buffer, (_keys_count + 7) / 8);

  // Let name be nullptr if track has no name. Allows to avoid allocating this
  // buffer in the constructor of empty animations.
  name_ =
      _name_len > 0 ? fill_span<char>(buffer, _name_len + 1).data() : nullptr;

  assert(buffer.empty() && "Whole buffer should be consumned");
}

template <typename _ValueType>
void Track<_ValueType>::Deallocate() {
  // Deallocate everything at once.
  memory::default_allocator()->Deallocate(as_writable_bytes(values_).data());

  values_ = {};
  ratios_ = {};
  steps_ = {};
  name_ = nullptr;
}

template <typename _ValueType>
size_t Track<_ValueType>::size() const {
  const size_t size = sizeof(*this) + values_.size_bytes() +
                      ratios_.size_bytes() + steps_.size_bytes();
  return size;
}

template <typename _ValueType>
void Track<_ValueType>::Save(ozz::io::OArchive& _archive) const {
  uint32_t num_keys = static_cast<uint32_t>(ratios_.size());
  _archive << num_keys;

  const size_t name_len = name_ ? eastl::CharStrlen(name_) : 0;
  _archive << static_cast<int32_t>(name_len);

  _archive << ozz::io::MakeArray(ratios_);
  _archive << ozz::io::MakeArray(values_);
  _archive << ozz::io::MakeArray(steps_);

  _archive << ozz::io::MakeArray(name_, name_len);
}

template <typename _ValueType>
void Track<_ValueType>::Load(ozz::io::IArchive& _archive, uint32_t _version) {
  // Destroy animation in case it was already used before.
  Deallocate();

  if (_version > 1) {
    LOGF(LogLevel::eERROR, "Unsupported Track version %d.", _version);
    return;
  }

  uint32_t num_keys;
  _archive >> num_keys;

  int32_t name_len;
  _archive >> name_len;

  Allocate(num_keys, name_len);

  _archive >> ozz::io::MakeArray(ratios_);
  _archive >> ozz::io::MakeArray(values_);
  _archive >> ozz::io::MakeArray(steps_);

  if (name_) {  // NULL name_ is supported.
    _archive >> ozz::io::MakeArray(name_, name_len);
    name_[name_len] = 0;
  }
}

// Explicitly instantiate supported tracks.
template class Track<float>;
//CONFFX_BEGIN
template class Track<Vector2>;
template class Track<Vector3>;
template class Track<Vector4>;
template class Track<Quat>;
//CONFFX_END
}  // namespace internal
}  // namespace animation
}  // namespace ozz
