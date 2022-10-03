The Better String Library

The Better String Library is an abstraction of a string data type which is
superior to the C library char buffer string type, or C++'s std::string.
Among the features achieved are:

  - Substantial mitigation of buffer overflow/overrun problems and other
    failures that result from erroneous usage of the common C string
    library functions

  - Significantly simplified string manipulation

  - High performance interoperability with other source/libraries which
    expect '\0' terminated char buffers

  - Improved overall performance of common string operations

  - Functional equivalency with other more modern languages

The library is totally stand alone, portable (known to work with gcc/g++,
MSVC++, Intel C++, WATCOM C/C++, Turbo C, Borland C++, IBM's native CC
compiler on Windows, Linux and Mac OS X), high performance, easy to use and
is not part of some other collection of data structures. Even the file I/O
functions are totally abstracted (so that other stream-like mechanisms, like
sockets, can be used.) Nevertheless, it is adequate as a complete
replacement of the C string library for string manipulation in any C program.

The library includes a robust C++ wrapper that uses overloaded operators,
rich constructors, exceptions, stream I/O and STL to make the CBString
struct a natural and powerful string abstraction with more functionality and
higher performance than std::string.

Bstrlib is stable, well tested and suitable for any software production
environment.
