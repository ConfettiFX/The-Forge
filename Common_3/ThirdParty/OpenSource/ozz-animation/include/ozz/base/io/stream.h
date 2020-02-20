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

#ifndef OZZ_OZZ_BASE_IO_STREAM_H_
#define OZZ_OZZ_BASE_IO_STREAM_H_

// Provides Stream interface used to read/write a memory buffer or a file with
// Crt fread/fwrite/fseek/ftell like functions.

#include "../../../../../../../OS/Interfaces/IFileSystem.h"
#include "../../../../../../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../../../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../platform.h"

#include <cstddef>

namespace ozz {
namespace io {

// Declares a stream access interface that conforms with CRT FILE API.
// This interface should be used to remap io operations.
class Stream {
 public:
  // Tests whether a file is opened.
  virtual bool opened() const = 0;

  // Reads _size bytes of data to _buffer from the stream. _buffer must be big
  // enough to store _size bytes. The position indicator of the stream is
  // advanced by the total amount of bytes read.
  // Returns the number of bytes actually read, which may be less than _size.
  virtual size_t Read(void* _buffer, size_t _size) = 0;

  // Writes _size bytes of data from _buffer to the stream. The position
  // indicator of the stream is advanced by the total number of bytes written.
  // Returns the number of bytes actually written, which may be less than
  // _size.
  virtual size_t Write(const void* _buffer, size_t _size) = 0;

  // Declares seeking origin enumeration.
  enum Origin {
    kCurrent,  // Current position of the stream pointer.
    kEnd,      // End of stream.
    kSet,      // Beginning of stream.
  };
  // Sets the position indicator associated with the stream to a new position
  // defined by adding _offset to a reference position specified by _origin.
  // Returns a zero value if successful, otherwise returns a non-zero value.
  virtual int Seek(int _offset, Origin _origin) = 0;

  // Returns the current value of the position indicator of the stream.
  // Returns -1 if an error occurs.
  virtual int FileTell() = 0;

  // Returns the current size of the stream.
    virtual size_t Size() = 0;

  //====================================================================

 protected:
  Stream() {}

  // Required virtual destructor.
  virtual ~Stream() {}

 private:
  Stream(const Stream&);
  void operator=(const Stream&);
};

// Implements Stream of type File.
class File : public Stream {
 public:
  // Test if a file at path _filename exists.
  // Note that this function is costly. If you aim to open the file right
  // after, then open it and use File::opened() to test if it's actually
  // existing.
  bool Exist(const char* _filename);

  // Open a file at path _filename with mode * _mode, in conformance with
  // fopen specifications. Use opened() function to test opening result.
  File(const char* _filename, const char* _mode);
    
  File(const Path* path, FileMode mode);

  // Gives _file ownership to the FileStream, which will be in charge of
  // closing it. _file must be NULL or a valid FileStream pointer.
  explicit File(void* _file);

  // Close the file if it is opened.
  virtual ~File();

  // Close the file if it is opened.
  bool CloseOzzFile();

  // See Stream::opened for details.
  virtual bool opened() const;

  // See Stream::Read for details.
  virtual size_t Read(void* _buffer, size_t _size);

  // See Stream::Write for details.
  virtual size_t Write(const void* _buffer, size_t _size);

  // See Stream::Seek for details.
  virtual int Seek(int _offset, Origin _origin);

  // See Stream::Tell for details.
  virtual int FileTell();

  // See Stream::Tell for details.
  virtual size_t Size();

 private:
  // The CRT file pointer.
  void* file_;	//Can be used as FileStream
};

// Implements an in-memory Stream. Allows to use a memory buffer as a Stream.
// The opening mode is equivalent to fopen w+b (binary read/write).
class MemoryStream : public Stream {
 public:
  // Construct an empty memory stream opened in w+b mode.
  MemoryStream();

  // Closes the stream and deallocates memory buffer.
  virtual ~MemoryStream();

  // See Stream::opened for details.
  virtual bool opened() const;

  // See Stream::Read for details.
  virtual size_t Read(void* _buffer, size_t _size);

  // See Stream::Write for details.
  virtual size_t Write(const void* _buffer, size_t _size);

  // See Stream::Seek for details.
  virtual int Seek(int _offset, Origin _origin);

  // See Stream::Tell for details.
  virtual int FileTell();

  // See Stream::Tell for details.
  virtual size_t Size();

  // Resizes buffers size to _size bytes. If _size is less than the actual
  // buffer size, then it remains unchanged.
  // Returns true if the buffer can contains _size bytes.
  bool Resize(size_t _size);

  // Size of the buffer increment.
  static const size_t kBufferSizeIncrement;

  // Maximum stream size.
  static const size_t kMaxSize;

  // Buffer of data.
  char* buffer_;

  // The size of the buffer, which is greater or equal to the size of the data
  // it contains (end_).
  size_t alloc_size_;

  // The effective size of the data in the buffer.
  int end_;

  // The cursor position in the buffer of data.
  int tell_;
};
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_IO_STREAM_H_
