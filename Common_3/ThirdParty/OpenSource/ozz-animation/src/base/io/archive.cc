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

#include "ozz/base/io/archive.h"

#include <cassert>

namespace ozz {
namespace io {

// OArchive implementation.

OArchive::OArchive(FileStream* _stream, Endianness _endianness)
    : stream_(_stream), endian_swap_(_endianness != GetNativeEndianness()) {
  assert(stream_ && stream_->pIO &&
         "_stream argument must point a valid opened stream.");
  // Save as a single byte as it does not need to be swapped.
  uint8_t endianness = static_cast<uint8_t>(_endianness);
  *this << endianness;
}

// IArchive implementation.

IArchive::IArchive(FileStream* _stream) : stream_(_stream), endian_swap_(false) {
  assert(stream_ && stream_->pIO &&
         "_stream argument must point a valid opened stream.");
  // Endianness was saved as a single byte, as it does not need to be swapped.
  uint8_t endianness;
  *this >> endianness;
  endian_swap_ = endianness != GetNativeEndianness();
}

}  // namespace io
}  // namespace ozz
