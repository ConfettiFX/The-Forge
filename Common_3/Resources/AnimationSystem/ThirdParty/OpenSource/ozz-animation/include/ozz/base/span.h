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

#ifndef OZZ_OZZ_BASE_SPAN_H_
#define OZZ_OZZ_BASE_SPAN_H_

#include "../../../include/ozz/base/platform.h"

namespace ozz {

// Defines a range [begin,end[ of objects ot type _Ty.
template <typename _Ty>
struct span {
  // Constants and types
  using element_type = _Ty;
  using value_type = _Ty;
  using index_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = _Ty*;
  using const_pointer = const _Ty*;
  using reference = _Ty&;
  using const_reference = const _Ty&;
  // Iterators
  using iterator = pointer;
  using const_iterator = const_pointer;

  // Default constructor initializes range to empty.
  span() : data_(NULL), size_(0) {}

  // Constructs a range from its extreme values.
  span(_Ty* _begin, _Ty* _end)
      : data_(_begin), size_(static_cast<size_t>(_end - _begin)) {
    ASSERT(_begin <= _end && "Invalid range.");
  }

  // Construct a range from a pointer to a buffer and its size, ie its number of
  // elements.
  span(_Ty* _begin, size_t _size) : data_(_begin), size_(_size) {}

  // Copy operator.
  void operator=(const span& _other) {
    data_ = _other.data_;
    size_ = _other.size_;
  }

  // Construct a range from a single element.
  explicit span(_Ty& _element) : data_(&_element), size_(1) {}

  // Construct a range from an array, its size is automatically deduced.
  // It isn't declared explicit as conversion is free and safe.
  template <size_t _size>
  span(_Ty (&_array)[_size]) : data_(_array), size_(_size) {}

  // Reinitialized from an array, its size is automatically deduced.
  template <size_t _size>
  void operator=(_Ty (&_array)[_size]) {
    data_ = _array;
    size_ = _size;
  }

  // Implement cast operator to allow conversions to span<const _Ty>.
  operator span<const _Ty>() const { return span<const _Ty>(data_, size_); }

  // Subspan

  span<element_type> first(index_type _count) const {
    ASSERT(_count <= size_ && "Count out of range");
    return {data(), _count};
  }

  span<element_type> last(index_type _count) const {
    ASSERT(_count <= size_ && "Count out of range");
    return {data() + size_ - _count, _count};
  }

  span<element_type> subspan(index_type _offset, index_type _count) const {
    ASSERT(_offset <= size_ && "Offset out of range");
    ASSERT(_count <= size_ && "Count out of range");
    ASSERT(_offset <= size_ - _count && "Offset + count out of range");
    return {data_ + _offset, _count};
  }

  // Returns a const reference to element _i of range [begin,end[.
  _Ty& operator[](size_t _i) const {
    ASSERT(_i < size_ && "Index out of range.");
    return data_[_i];
  }

  bool empty() const { return size_ == 0; }

  // Complies with other contiguous containers.
  _Ty* data() const { return data_; }

  // Gets the number of elements of the range.
  // This size isn't stored but computed from begin and end pointers.
  size_t size() const { return size_; }

  // Gets the size in byte of the range.
  size_t size_bytes() const { return size_ * sizeof(element_type); }

  // Iterator support
  iterator begin() const { return data_; }
  iterator end() const { return data_ + size_; }

 private:
  // span begin pointer.
  _Ty* data_;

  // span end pointer, should never be dereferenced.
  size_t size_;
};

// Returns a span from an array.
template <typename _Ty, size_t _Size>
inline span<_Ty> make_span(_Ty (&_arr)[_Size]) {
  return {_arr};
}

// Returns a mutable span from a container.
template <typename _Container>
inline span<typename _Container::value_type> make_span(_Container& _container) {
  return {_container.data(), _container.size()};
}

// Returns a non mutable span from a container.
template <typename _Container>
inline span<const typename _Container::value_type> make_span(
    const _Container& _container) {
  return {_container.data(), _container.size()};
}

// As bytes
template <typename _Ty>
inline span<const byte> as_bytes(const span<_Ty>& _span) {
  return {reinterpret_cast<const byte*>(_span.data()), _span.size_bytes()};
}

template <typename _Ty>
inline span<byte> as_writable_bytes(const span<_Ty>& _span) {
  // Compilation will fail here if _Ty is const. This prevents from writing to
  // const data.
  return {reinterpret_cast<byte*>(_span.data()), _span.size_bytes()};
}

// Fills a typed span from a byte source span. Source byte span is modified to
// reflect remain size.
template <typename _Ty>
inline span<_Ty> fill_span(span<byte>& _src, size_t _count) {
  ASSERT(ozz::IsAligned(_src.data(), alignof(_Ty)) && "Invalid alignment.");
  const span<_Ty> ret = {reinterpret_cast<_Ty*>(_src.data()), _count};
  // Validity assertion is done by span constructor.
  _src = {reinterpret_cast<byte*>(ret.end()), _src.end()};
  return ret;
}

}  // namespace ozz
#endif  // OZZ_OZZ_BASE_SPAN_H_
