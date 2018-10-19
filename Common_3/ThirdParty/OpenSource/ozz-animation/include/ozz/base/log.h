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

#ifndef OZZ_OZZ_BASE_LOG_H_
#define OZZ_OZZ_BASE_LOG_H_

#include <iostream>

// MSVC includes <cstring> from <iostream> but not gcc.
// So it is included here to ensure a portable behavior.
#include <cstring>

// Proposes a logging interface that redirects logs to std::cout, clog and cerr
// output streams. This interface adds a logging level functionality (kSilent,
// kStandard, kVerbose) to the std API, which can be set using
// ozz::log::GetLevel function.
// Usage conforms to std stream usage: ozz::log::OUT() << "something to log."...

namespace ozz {
namespace log {

enum Level {
  kSilent,    // No output at all, even errors are muted.
  kStandard,  // Default output level.
  kVerbose,   // Most verbose output level.
};

// Sets the global logging level.
Level SetLevel(Level _level);

// Gets the global logging level.
Level GetLevel();

namespace internal {

// Implements logging base class.
// This class is not intended to be used publicly, it is derived by user
// classes LogV, Log, Out, Err...
// Forwards ostream::operator << to a standard ostream or a silent
// ostringstream according to the logging level at construction time.
class Logger {
 public:
  // Forwards ostream::operator << for any type.
  template <typename _T>
  std::ostream& operator<<(const _T& _t) {
    return stream_ << _t;
  }
  // Forwards ostream::operator << for modifiers.
  std::ostream& operator<<(std::ostream& (*_Pfn)(std::ostream&)) {
    return ((*_Pfn)(stream_));
  }
  // Cast operator.
  operator std::ostream&() { return stream_; }

  // Cast operator.
  std::ostream& stream() { return stream_; }

 protected:
  // Specifies the global stream and the output level.
  // Logging levels allows to select _stream or a "silent" stream according to
  // the current global logging level.
  Logger(std::ostream& _stream, Level _level);

  // Destructor, deletes the internal "silent" stream.
  ~Logger();

 private:
  // Disables copy and assignment.
  Logger(const Logger&);
  void operator=(Logger&);

  // Selected output stream.
  std::ostream& stream_;

  // Stores whether the stream is local one, in which case it must be deleted
  // in the destructor.
  bool local_stream_;
};
}  // namespace internal

// Logs verbose output to the standard error stream (std::clog).
// Enabled if logging level is Verbose.
class LogV : public internal::Logger {
 public:
  LogV();
};

// Logs output to the standard error stream (std::clog).
// Enabled if logging level is not Silent.
class Log : public internal::Logger {
 public:
  Log();
};

// Logs output to the standard output (std::cout).
// Enabled if logging level is not Silent.
class Out : public internal::Logger {
 public:
  Out();
};

// Logs error to the standard error stream (std::cerr).
// Enabled if logging level is not Silent.
class Err : public internal::Logger {
 public:
  Err();
};
}  // namespace log
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_LOG_H_
