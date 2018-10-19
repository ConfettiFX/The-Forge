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

#include "ozz/base/io/stream.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>

#include "ozz/base/maths/math_ex.h"
#include "ozz/base/memory/allocator.h"

namespace ozz {
namespace io {

// Starts File implementation.

bool File::Exist(const char* _filename) {
  FILE* file = std::fopen(_filename, "r");
  if (file) {
    std::fclose(file);
    return true;
  }
  return false;
}

File::File(const char* _filename, const char* _mode)
    : file_(std::fopen(_filename, _mode)) {}

File::File(void* _file) : file_(_file) {}

File::~File() { Close(); }

void File::Close() {
  if (file_) {
    std::FILE* file = reinterpret_cast<std::FILE*>(file_);
    std::fclose(file);
    file_ = NULL;
  }
}

bool File::opened() const { return file_ != NULL; }

size_t File::Read(void* _buffer, size_t _size) {
  std::FILE* file = reinterpret_cast<std::FILE*>(file_);
  return std::fread(_buffer, 1, _size, file);
}

size_t File::Write(const void* _buffer, size_t _size) {
  std::FILE* file = reinterpret_cast<std::FILE*>(file_);
  return std::fwrite(_buffer, 1, _size, file);
}

int File::Seek(int _offset, Origin _origin) {
  int origins[] = {SEEK_CUR, SEEK_END, SEEK_SET};
  if (_origin >= static_cast<int>(OZZ_ARRAY_SIZE(origins))) {
    return -1;
  }
  std::FILE* file = reinterpret_cast<std::FILE*>(file_);
  return std::fseek(file, _offset, origins[_origin]);
}

int File::Tell() const {
  std::FILE* file = reinterpret_cast<std::FILE*>(file_);
  return (int)std::ftell(file);
}

size_t File::Size() const {
  std::FILE* file = reinterpret_cast<std::FILE*>(file_);

  const int current = (int)std::ftell(file);
  assert(current >= 0);
  int seek = std::fseek(file, 0, SEEK_END);
  assert(seek == 0);
  (void)seek;
  const int end = (int)std::ftell(file);
  assert(end >= 0);
  seek = std::fseek(file, current, SEEK_SET);
  assert(seek == 0);

  return static_cast<size_t>(end);
}

// Starts MemoryStream implementation.
const size_t MemoryStream::kBufferSizeIncrement = 16 << 10;
const size_t MemoryStream::kMaxSize = std::numeric_limits<int>::max();

MemoryStream::MemoryStream()
    : buffer_(NULL), alloc_size_(0), end_(0), tell_(0) {}

MemoryStream::~MemoryStream() {
  ozz::memory::default_allocator()->Deallocate(buffer_);
  buffer_ = NULL;
}

bool MemoryStream::opened() const { return true; }

size_t MemoryStream::Read(void* _buffer, size_t _size) {
  // A read cannot set file position beyond the end of the file.
  // A read cannot exceed the maximum Stream size.
  if (tell_ > end_ || _size > kMaxSize) {
    return 0;
  }

  const int read_size = math::Min(end_ - tell_, static_cast<int>(_size));
  std::memcpy(_buffer, buffer_ + tell_, read_size);
  tell_ += read_size;
  return read_size;
}

size_t MemoryStream::Write(const void* _buffer, size_t _size) {
  if (_size > kMaxSize || tell_ > static_cast<int>(kMaxSize - _size)) {
    // A write cannot exceed the maximum Stream size.
    return 0;
  }
  if (tell_ > end_) {
    // The fseek() function shall allow the file-position indicator to be set
    // beyond the end of existing data in the file. If data is later written at
    // this point, subsequent reads of data in the gap shall return bytes with
    // the value 0 until data is actually written into the gap.
    if (!Resize(tell_)) {
      return 0;
    }
    // Fills the gap with 0's.
    const size_t gap = tell_ - end_;
    std::memset(buffer_ + end_, 0, gap);
    end_ = tell_;
  }

  const int size = static_cast<int>(_size);
  const int tell_end = tell_ + size;
  if (Resize(tell_end)) {
    end_ = math::Max(tell_end, end_);
    std::memcpy(buffer_ + tell_, _buffer, _size);
    tell_ += size;
    return _size;
  }
  return 0;
}

int MemoryStream::Seek(int _offset, Origin _origin) {
  int origin;
  switch (_origin) {
    case kCurrent:
      origin = tell_;
      break;
    case kEnd:
      origin = end_;
      break;
    case kSet:
      origin = 0;
      break;
    default:
      return -1;
  }

  // Exit if seeking before file begin or beyond max file size.
  if (origin < -_offset ||
      (_offset > 0 && origin > static_cast<int>(kMaxSize - _offset))) {
    return -1;
  }

  // So tell_ is moved but end_ pointer is not moved until something is later
  // written.
  tell_ = origin + _offset;
  return 0;
}

int MemoryStream::Tell() const { return tell_; }

size_t MemoryStream::Size() const { return static_cast<size_t>(end_); }

bool MemoryStream::Resize(size_t _size) {
  if (_size > alloc_size_) {
    // Resize to the next multiple of kBufferSizeIncrement, requires
    // kBufferSizeIncrement to be a power of 2.
    OZZ_STATIC_ASSERT(
        (MemoryStream::kBufferSizeIncrement & (kBufferSizeIncrement - 1)) == 0);

    alloc_size_ = ozz::math::Align(_size, kBufferSizeIncrement);
    buffer_ =
        ozz::memory::default_allocator()->Reallocate(buffer_, alloc_size_);
  }
  return _size == 0 || buffer_ != NULL;
}
}  // namespace io
}  // namespace ozz
