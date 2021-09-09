/************************************************************************************

Filename    :   OVR_UTF8Util.h
Content     :   UTF8 Unicode character encoding/decoding support
Created     :   September 19, 2012
Notes       :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/
#pragma once

#include <string>

namespace OVRFW {
namespace UTF8Util {

// *** UTF8 string length and indexing.

// Determines the length of UTF8 string in characters.
// If source length is specified (in bytes), null 0 character is counted properly.
intptr_t GetLength(const char* putf8str, intptr_t length = -1);

// Gets a decoded UTF8 character at index; you can access up to the index returned
// by GetLength. 0 will be returned for out of bounds access.
uint32_t GetCharAt(intptr_t index, const char* putf8str, intptr_t length = -1);

// Converts UTF8 character index into byte offset.
// -1 is returned if index was out of bounds.
intptr_t GetByteIndex(intptr_t index, const char* putf8str, intptr_t length = -1);

// *** 16-bit Unicode string Encoding/Decoding routines.

// Determines the number of bytes necessary to encode a string.
// Does not count the terminating 0 (null) character.
intptr_t GetEncodeStringSize(const wchar_t* pchar, intptr_t length = -1);

// Encodes a unicode (UCS-2 only) string into a buffer. The size of buffer must be at
// least GetEncodeStringSize() + 1.
void EncodeString(char* pbuff, const wchar_t* pchar, intptr_t length = -1);

// Decode UTF8 into a wchar_t buffer. Must have GetLength()+1 characters available.
// Characters over 0xFFFF are replaced with 0xFFFD.
// Returns the length of resulting string (number of characters)
size_t DecodeString(wchar_t* pbuff, const char* putf8str, intptr_t bytesLen = -1);

// *** Individual character Encoding/Decoding.

// Determined the number of bytes necessary to encode a UCS character.
int GetEncodeCharSize(uint32_t ucsCharacter);

// Encodes the given UCS character into the given UTF-8 buffer.
// Writes the data starting at buffer[offset], and
// increments offset by the number of bytes written.
// May write up to 6 bytes, so make sure there's room in the buffer
void EncodeChar(char* pbuffer, intptr_t* poffset, uint32_t ucsCharacter);

// Return the next Unicode character in the UTF-8 encoded buffer.
// Invalid UTF-8 sequences produce a U+FFFD character as output.
// Advances *utf8_buffer past the character returned. Pointer advance
// occurs even if the terminating 0 character is hit, since that allows
// strings with middle '\0' characters to be supported.
uint32_t DecodeNextChar_Advance0(const char** putf8Buffer);

// Safer version of DecodeNextChar, which doesn't advance pointer if
// null character is hit.
inline uint32_t DecodeNextChar(const char** putf8Buffer) {
    uint32_t ch = DecodeNextChar_Advance0(putf8Buffer);
    if (ch == 0)
        (*putf8Buffer)--;
    return ch;
}

bool DecodePrevChar(char const* p, intptr_t& offset, uint32_t& ch);

void AppendChar(std::string& s, uint32_t ch);

} // namespace UTF8Util
} // namespace OVRFW
