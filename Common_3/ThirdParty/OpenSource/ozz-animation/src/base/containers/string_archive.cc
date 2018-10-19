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

#include "ozz/base/containers/string_archive.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_ex.h"

namespace ozz {
namespace io {
void Extern<String::Std>::Save(OArchive& _archive, const String::Std* _values,
                               size_t _count) {
  for (size_t i = 0; i < _count; i++) {
    const ozz::String::Std& string = _values[i];

    // Get size excluding null terminating character.
    uint32_t size = static_cast<uint32_t>(string.size());
    _archive << size;
    _archive << ozz::io::MakeArray(string.c_str(), size);
  }
}

void Extern<String::Std>::Load(IArchive& _archive, String::Std* _values,
                               size_t _count, uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; i++) {
    ozz::String::Std& string = _values[i];

    // Ensure an existing string is reseted.
    string.clear();

    uint32_t size;
    _archive >> size;
    string.reserve(size);

    // Prepares temporary buffer used for reading.
    char buffer[128];
    for (size_t to_read = size; to_read != 0;) {
      // Read from the archive to the local temporary buffer.
      const size_t to_read_this_loop =
          math::Min(to_read, OZZ_ARRAY_SIZE(buffer));
      _archive >> ozz::io::MakeArray(buffer, to_read_this_loop);
      to_read -= to_read_this_loop;

      // Append to the string.
      string.append(buffer, to_read_this_loop);
    }
  }
}
}  // namespace io
}  // namespace ozz
