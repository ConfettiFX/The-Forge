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

#include "../../../include/ozz/animation/runtime/track.h"



#include "../../../include/ozz/base/io/archive.h"
#include "../../../include/ozz/base/maths/math_archive.h"
#include "../../../include/ozz/base/maths/simd_math_archive.h"
#include "../../../include/ozz/base/maths/math_ex.h"
#include "../../../include/ozz/base/memory/allocator.h"

namespace ozz {
namespace animation {

namespace internal {

template <typename _ValueType>
Track<_ValueType>::Track() : name_(NULL) {}

template <typename _ValueType>
Track<_ValueType>::Track(Track<_ValueType>&& _other) {
  *this = std::move(_other);
}

template <typename _ValueType>
Track<_ValueType>& Track<_ValueType>::operator=(Track<_ValueType>&& _other) {
  std::swap(ratios_, _other.ratios_);
  std::swap(values_, _other.values_);
  std::swap(steps_, _other.steps_);
  std::swap(name_, _other.name_);
  return *this;
}

template <typename _ValueType>
Track<_ValueType>::~Track() {
  Deallocate();
}

template <typename _ValueType>
void Track<_ValueType>::Allocate(size_t _keys_count, size_t _name_len) {
  ASSERT(ratios_.size() == 0 && values_.size() == 0);

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
  span<byte> buffer = {static_cast<byte*>(memory::default_allocator()->Allocate(
                           buffer_size, alignof(_ValueType))),
                       buffer_size};

  // Fix up pointers. Serves larger alignment values first.
  values_ = fill_span<_ValueType>(buffer, _keys_count);
  ratios_ = fill_span<float>(buffer, _keys_count);
  steps_ = fill_span<uint8_t>(buffer, (_keys_count + 7) / 8);

  // Let name be NULL if track has no name. Allows to avoid allocating this
  // buffer in the constructor of empty animations.
  name_ =
      _name_len > 0 ? fill_span<char>(buffer, _name_len + 1).data() : NULL;

  ASSERT(buffer.empty() && "Whole buffer should be consumned");
}

template <typename _ValueType>
void Track<_ValueType>::Deallocate() {
  // Deallocate everything at once.
  memory::default_allocator()->Deallocate(as_writable_bytes(values_).data());

  values_ = {};
  ratios_ = {};
  steps_ = {};
  name_ = NULL;
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

  const size_t name_len = name_ ? strlen(name_) : 0;
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
    LOGF(eERROR, "Unsupported Track version %d.", _version);
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
template class OZZ_ANIMATION_DLL Track<float>;
template class OZZ_ANIMATION_DLL Track<Vector2>;
template class OZZ_ANIMATION_DLL Track<Vector3>;
template class OZZ_ANIMATION_DLL Track<Vector4>;
template class OZZ_ANIMATION_DLL Track<Quat>;

}  // namespace internal
}  // namespace animation
}  // namespace ozz
