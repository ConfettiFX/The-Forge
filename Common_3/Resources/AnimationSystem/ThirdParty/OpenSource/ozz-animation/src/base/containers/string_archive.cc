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

#include "../../../include/ozz/base/containers/string_archive.h"

#include "../../../include/ozz/base/io/archive.h"
#include "../../../include/ozz/base/maths/math_ex.h"

namespace ozz {
namespace io {

    void Extern<bstring>::Save(OArchive& _archive, const bstring* _values,
        size_t _count) {
        for (size_t i = 0; i < _count; i++) {
            const bstring& string = _values[i];

            // Get size excluding null terminating character.
            uint32_t size = blength(&string);
            _archive << size;
            _archive << ozz::io::MakeArray((const char*)string.data, size);
        }
    }

    void Extern<bstring>::Load(IArchive& _archive, bstring* _values, size_t _count,
        uint32_t _version) {
        (void)_version;
        for (size_t i = 0; i < _count; i++) {
            bstring& string = _values[i];

            uint32_t size;
            _archive >> size;

            // Prepares temporary buffer used for reading.
            char buffer[128];
            for (size_t to_read = size; to_read != 0;) {
                // Read from the archive to the local temporary buffer.
                const size_t to_read_this_loop =
                    math::Min(to_read, OZZ_ARRAY_SIZE(buffer)-1);
                _archive >> ozz::io::MakeArray(buffer, to_read_this_loop);
                to_read -= to_read_this_loop;
                buffer[to_read_this_loop] = 0;

                // Append to the string.
                bcatcstr(&string, buffer);
            }
        }
    }

}  // namespace io
}  // namespace ozz
