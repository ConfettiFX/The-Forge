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

#ifndef OZZ_OZZ_BASE_IO_ARCHIVE_H_
#define OZZ_OZZ_BASE_IO_ARCHIVE_H_

// Provides input (IArchive) and output (OArchive) serialization containers.
// Archive are similar to c++ iostream. Data can be saved to a OArchive with
// the << operator, or loaded from a IArchive with the >> operator.
// Primitive data types are simply saved/loaded to/from archives, while struct
// and class are saved/loaded through Save/Load intrusive or non-intrusive
// functions:
// - The intrusive function prototypes are "void Save(ozz::io::OArchive*) const"
// and "void Load(ozz::io::IArchive*)".
// - The non-intrusive functions allow to work on arrays of objects. They must
// be implemented in ozz::io namespace, by specializing the following template
// struct:
// template <typename _Ty>
// struct Extern {
//   static void Save(OArchive& _archive, const _Ty* _ty, size_t _count);
//   static void Load(IArchive& _archive, _Ty* _ty, size_t _count,
//                    uint32_t _version);
// };
//
// Arrays of struct/class or primitive types can be saved/loaded with the
// helper function ozz::io::MakeArray() that is then streamed in or out using
// << and >> archive operators: archive << ozz::io::MakeArray(my_array, count);
//
// Versioning can be done using OZZ_IO_TYPE_VERSION macros. Type version
// is saved in the OArchive, and is given back to Load functions to allow to
// manually handle version modifications. Versioning can be disabled using
// OZZ_IO_TYPE_NOT_VERSIONABLE like macros. It can not be re-enabled afterward.
//
// Objects can be assigned a tag using OZZ_IO_TYPE_TAG macros. A tag allows to
// check the type of the next object to read from an archive. An automatic
// assertion check is performed for each object that has a tag. It can also be
// done manually to ensure an archive has the expected content.
//
// Endianness (big-endian or little-endian) can be specified while constructing
// an output archive (ozz::io::OArchive). Input archives automatically handle
// endianness conversion if the native platform endian mode differs from the
// archive one.
//
// IArchive and OArchive expect valid streams as argument, respectively opened
// for reading and writing. Archives do NOT perform error detection while
// reading or writing. All errors are considered programming errors. This leads
// to the following assertions on the user side:
// - When writing: the stream must be big (or grow-able) enough to support the
// data being written.
// - When reading: Stream's tell (position in the stream) must match the object
// being read. To help with this requirement, archives provide a tag mechanism
// that allows to check the tag (ie the type) of the next object to read. Stream
// integrity, like data corruption or file truncation, must also be validated on
// the user side.

#include <stdint.h>



#include "../../../../include/ozz/base/endianness.h"
#include "../../../../include/ozz/base/io/archive_traits.h"
#include "../../../../include/ozz/base/platform.h"
#include "../../../../include/ozz/base/span.h"

#include "../../../../../../../../../Utilities/Interfaces/IFileSystem.h"

namespace ozz {
namespace io {
namespace internal {
// Defines Tagger helper object struct.
// The boolean template argument is used to automatically select a template
// specialization, whether _Ty has a tag or not.
template <typename _Ty,
          bool _HasTag = internal::Tag<const _Ty>::kTagLength != 0>
struct Tagger;
}  // namespace internal

// Implements output archive concept used to save/serialize data from a Stream.
// The output endianness mode is set at construction time. It is written to the
// stream to allow the IArchive to perform the required conversion to the native
// endianness mode while reading.
class OZZ_BASE_DLL OArchive {
 public:
  // Constructs an output archive from the Stream _stream that must be valid
  // and opened for writing.
  explicit OArchive(FileStream* _stream,
                    Endianness _endianness = GetNativeEndianness());

  // Returns true if an endian swap is required while writing.
  bool endian_swap() const { return endian_swap_; }

  // Saves _size bytes of binary data from _data.
  size_t SaveBinary(const void* _data, size_t _size) {
    return fsWriteToStream(stream_, _data, _size);
  }

  // Class type saving.
  template <typename _Ty>
  void operator<<(const _Ty& _ty) {
    internal::Tagger<const _Ty>::Write(*this);
    SaveVersion<_Ty>();
    Extern<_Ty>::Save(*this, &_ty, 1);
  }

// Primitive type saving.
#define OZZ_IO_PRIMITIVE_TYPE(_type)                              \
  void operator<<(_type _v) {                                     \
    _type v = endian_swap_ ? EndianSwapper<_type>::Swap(_v) : _v; \
    size_t size = fsWriteToStream(stream_, &v, sizeof(v));    \
    ASSERT(size == sizeof(v));                                    \
  }

  OZZ_IO_PRIMITIVE_TYPE(char)
  OZZ_IO_PRIMITIVE_TYPE(int8_t)
  OZZ_IO_PRIMITIVE_TYPE(uint8_t)
  OZZ_IO_PRIMITIVE_TYPE(int16_t)
  OZZ_IO_PRIMITIVE_TYPE(uint16_t)
  OZZ_IO_PRIMITIVE_TYPE(int32_t)
  OZZ_IO_PRIMITIVE_TYPE(uint32_t)
  OZZ_IO_PRIMITIVE_TYPE(int64_t)
  OZZ_IO_PRIMITIVE_TYPE(uint64_t)
  OZZ_IO_PRIMITIVE_TYPE(bool)
  OZZ_IO_PRIMITIVE_TYPE(float)
#undef OZZ_IO_PRIMITIVE_TYPE

  // Returns output stream.
      FileStream* stream() const { return stream_; }

 private:

    template < typename _Ty >
    struct SaveVersionIntennal
    {
        static void Save(OArchive* archive)
        {
            // Compilation could fail here if the version is not defined for _Ty, or if
            // the .h file containing its definition is not included by the caller of
            // this function.
            if (void(0), internal::Version<const _Ty>::kValue != 0) {
                uint32_t version = internal::Version<const _Ty>::kValue;
                *archive << version;
            }
        }
    };

    template < typename _Ty >
    struct SaveVersionIntennal<const _Ty*>
    {
        static void Save(OArchive* archive)
        {
            // Arrays don't have a version
        }
    };
    template < typename _Ty >
    struct SaveVersionIntennal< _Ty*>
    {
        static void Save(OArchive* archive)
        {
            UNREF_PARAM(archive); 
            // Arrays don't have a version
        }
    };

  template <typename _Ty>
  void SaveVersion() {
      SaveVersionIntennal<_Ty>::Save(this);
  }

  // The output stream.
  FileStream* stream_;

  // Endian swap state, true if a conversion is required while writing.
  bool endian_swap_;
};

// Implements input archive concept used to load/de-serialize data to a Stream.
// Endianness conversions are automatically performed according to the Archive
// and the native formats.
class OZZ_BASE_DLL IArchive {
 public:
  // Constructs an input archive from the Stream _stream that must be opened for
  // reading, at the same tell (position in the stream) as when it was passed to
  // the OArchive.
  explicit IArchive(FileStream* _stream);

  // Returns true if an endian swap is required while reading.
  bool endian_swap() const { return endian_swap_; }

  // Loads _size bytes of binary data to _data.
  size_t LoadBinary(void* _data, size_t _size) {
    return fsReadFromStream(stream_, _data, _size);
  }

  // Class type loading.
  template <typename _Ty>
  void operator>>(_Ty& _ty) {
    // Only uses tag validation for assertions, as reading cannot fail.
    bool valid = internal::Tagger<const _Ty>::Validate(*this);
    ASSERT(valid && "Type tag does not match archive content.");

    // Loads instance.
    uint32_t version = LoadVersion<_Ty>();
    Extern<_Ty>::Load(*this, &_ty, 1, version);
  }

// Primitive type loading.
#define OZZ_IO_PRIMITIVE_TYPE(_type)                          \
  void operator>>(_type& _v) {                                \
    _type v;                                                  \
    size_t size = fsReadFromStream(stream_, &v, sizeof(v)); \
    ASSERT(size == sizeof(v));                                \
    _v = endian_swap_ ? EndianSwapper<_type>::Swap(v) : v;    \
  }

  OZZ_IO_PRIMITIVE_TYPE(char)
  OZZ_IO_PRIMITIVE_TYPE(int8_t)
  OZZ_IO_PRIMITIVE_TYPE(uint8_t)
  OZZ_IO_PRIMITIVE_TYPE(int16_t)
  OZZ_IO_PRIMITIVE_TYPE(uint16_t)
  OZZ_IO_PRIMITIVE_TYPE(int32_t)
  OZZ_IO_PRIMITIVE_TYPE(uint32_t)
  OZZ_IO_PRIMITIVE_TYPE(int64_t)
  OZZ_IO_PRIMITIVE_TYPE(uint64_t)
  OZZ_IO_PRIMITIVE_TYPE(bool)
  OZZ_IO_PRIMITIVE_TYPE(float)
#undef OZZ_IO_PRIMITIVE_TYPE

  template <typename _Ty>
  bool TestTag() {
    // Only tagged types can be tested. If compilations fails here, it can
    // mean the file containing tag declaration is not included.
    static_assert(internal::Tag<const _Ty>::kTagLength != 0,
                  "Tag unknown for type.");

    ssize_t tell = fsGetStreamSeekPosition(stream_);
    bool valid = internal::Tagger<const _Ty>::Validate(*this);
    fsSeekStream(stream_, SeekBaseOffset::SBO_START_OF_FILE, tell);  // Rewinds before the tag test.
    return valid;
  }

  // Returns input stream.
  FileStream* stream() const { return stream_; }

 private:

     template < typename _Ty >
     struct LoadVersionIntennal
     {
         static uint32_t Save(IArchive* archive)
         {
             uint32_t version = 0;
             if (void(0), internal::Version<const _Ty>::kValue != 0) {
                 *archive >> version;
             }
             return version;
         }
     };

     template < typename _Ty >
     struct LoadVersionIntennal<const _Ty*>
     {
         static uint32_t Save(IArchive* archive)
         {
             // Arrays don't have a version
             return 0;
         }
     };

     template < typename _Ty >
     struct LoadVersionIntennal< _Ty*>
     {
         static uint32_t Save(IArchive* archive)
         {
             UNREF_PARAM(archive); 
             // Arrays don't have a version
             return 0;
         }
     };

  template <typename _Ty>
  uint32_t LoadVersion() {
      return LoadVersionIntennal<_Ty>::Save(this);
  }

  // The input stream.
  FileStream* stream_;

  // Endian swap state, true if a conversion is required while reading.
  bool endian_swap_;
};

// Primitive type are not versionable.
OZZ_IO_TYPE_NOT_VERSIONABLE(char)
OZZ_IO_TYPE_NOT_VERSIONABLE(int8_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(uint8_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(int16_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(uint16_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(int32_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(uint32_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(int64_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(uint64_t)
OZZ_IO_TYPE_NOT_VERSIONABLE(bool)
OZZ_IO_TYPE_NOT_VERSIONABLE(float)

// Default loading and saving external implementation.
template <typename _Ty>
struct Extern {
  inline static void Save(OArchive& _archive, const _Ty* _ty, size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      _ty[i].Save(_archive);
    }
  }
  inline static void Load(IArchive& _archive, _Ty* _ty, size_t _count,
                          uint32_t _version) {
    for (size_t i = 0; i < _count; ++i) {
      _ty[i].Load(_archive, _version);
    }
  }
};

// Wrapper for dynamic array serialization.
// Must be used through ozz::io::MakeArray.
namespace internal {
template <typename _Ty>
struct Array {
  OZZ_INLINE void Save(OArchive& _archive) const {
    ozz::io::Extern<_Ty>::Save(_archive, array, count);
  }
  OZZ_INLINE void Load(IArchive& _archive, uint32_t _version) const {
    ozz::io::Extern<_Ty>::Load(_archive, array, count, _version);
  }
  _Ty* array;
  size_t count;
};
// Specialize for const _Ty which can only be saved.
template <typename _Ty>
struct Array<const _Ty> {
  OZZ_INLINE void Save(OArchive& _archive) const {
    ozz::io::Extern<_Ty>::Save(_archive, array, count);
  }
  const _Ty* array;
  size_t count;
};

// Array copies version from the type it contains.
// Definition of Array of _Ty version: _Ty version.
template <typename _Ty>
struct Version<const Array<_Ty>> {
  enum { kValue = Version<const _Ty>::kValue };
};

// Specializes Array Save/Load for primitive types.
#define OZZ_IO_PRIMITIVE_TYPE(_type)                                        \
  template <>                                                               \
  inline void Array<const _type>::Save(OArchive& _archive) const {          \
    if (_archive.endian_swap()) {                                           \
      /* Save element by element as swapping in place the whole buffer is*/ \
      /* not possible.*/                                                    \
      for (size_t i = 0; i < count; ++i) {                                  \
        _archive << array[i];                                               \
      }                                                                     \
    } else {                                                                \
      size_t size =                                           \
      _archive.SaveBinary(array, count * sizeof(_type));                    \
      ASSERT(size == count * sizeof(_type));                                \
    }                                                                       \
  }                                                                         \
                                                                            \
  template <>                                                               \
  inline void Array<_type>::Save(OArchive& _archive) const {                \
    if (_archive.endian_swap()) {                                           \
      /* Save element by element as swapping in place the whole buffer is*/ \
      /* not possible.*/                                                    \
      for (size_t i = 0; i < count; ++i) {                                  \
        _archive << array[i];                                               \
      }                                                                     \
    } else {                                                                \
      size_t size =                                           \
      _archive.SaveBinary(array, count * sizeof(_type));                    \
      ASSERT(size == count * sizeof(_type));                                \
    }                                                                       \
  }                                                                         \
                                                                            \
  template <>                                                               \
  inline void Array<_type>::Load(IArchive& _archive, uint32_t /*_version*/) \
      const {                                                               \
    size_t size =                                             \
    _archive.LoadBinary(array, count * sizeof(_type));                      \
    ASSERT(size == count * sizeof(_type));                                  \
    if (_archive.endian_swap()) { /*Can swap in-place.*/                    \
      EndianSwapper<_type>::Swap(array, count);                             \
    }                                                                       \
  }

OZZ_IO_PRIMITIVE_TYPE(char)
OZZ_IO_PRIMITIVE_TYPE(int8_t)
OZZ_IO_PRIMITIVE_TYPE(uint8_t)
OZZ_IO_PRIMITIVE_TYPE(int16_t)
OZZ_IO_PRIMITIVE_TYPE(uint16_t)
OZZ_IO_PRIMITIVE_TYPE(int32_t)
OZZ_IO_PRIMITIVE_TYPE(uint32_t)
OZZ_IO_PRIMITIVE_TYPE(int64_t)
OZZ_IO_PRIMITIVE_TYPE(uint64_t)
OZZ_IO_PRIMITIVE_TYPE(bool)
OZZ_IO_PRIMITIVE_TYPE(float)
#undef OZZ_IO_PRIMITIVE_TYPE
}  // namespace internal

// Utility function that instantiates Array wrapper.
template <typename _Ty>
OZZ_INLINE const internal::Array<_Ty> MakeArray(_Ty* _array, size_t _count) {
  const internal::Array<_Ty> array = {_array, _count};
  return array;
}
template <typename _Ty>
OZZ_INLINE const internal::Array<const _Ty> MakeArray(const _Ty* _array,
                                                      size_t _count) {
  const internal::Array<const _Ty> array = {_array, _count};
  return array;
}
template <typename _Ty>
OZZ_INLINE const internal::Array<_Ty> MakeArray(span<_Ty> _array) {
  const internal::Array<_Ty> array = {_array.data(), _array.size()};
  return array;
}
template <typename _Ty>
OZZ_INLINE const internal::Array<const _Ty> MakeArray(span<const _Ty> _array) {
  const internal::Array<const _Ty> array = {_array.data(), _array.size()};
  return array;
}
template <typename _Ty, size_t _count>
OZZ_INLINE const internal::Array<_Ty> MakeArray(_Ty (&_array)[_count]) {
  const internal::Array<_Ty> array = {_array, _count};
  return array;
}
template <typename _Ty, size_t _count>
OZZ_INLINE const internal::Array<const _Ty> MakeArray(
    const _Ty (&_array)[_count]) {
  const internal::Array<const _Ty> array = {_array, _count};
  return array;
}

namespace internal {
// Specialization of the Tagger helper for tagged types.
template <typename _Ty>
struct Tagger<_Ty, true> {
  static void Write(OArchive& _archive) {
    typedef internal::Tag<const _Ty> Tag;
    size_t size =
    _archive.SaveBinary(Tag::Get(), Tag::kTagLength);
    ASSERT(size == Tag::kTagLength);
  }
  static bool Validate(IArchive& _archive) {
    typedef internal::Tag<const _Ty> Tag;
    char buf[Tag::kTagLength];
    if (Tag::kTagLength != _archive.LoadBinary(buf, Tag::kTagLength)) {
      return false;
    }
    const char* tag = Tag::Get();
    size_t i = 0;
    for (; i < Tag::kTagLength && buf[i] == tag[i]; ++i) {
    }
    return i == Tag::kTagLength;
  }
};

// Specialization of the Tagger helper for types with no tag.
template <typename _Ty>
struct Tagger<_Ty, false> {
  static void Write(OArchive& /*_archive*/) {}
  static bool Validate(IArchive& /*_archive*/) { return true; }
};
}  // namespace internal
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_IO_ARCHIVE_H_
