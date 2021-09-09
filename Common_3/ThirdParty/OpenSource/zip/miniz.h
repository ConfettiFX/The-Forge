/*
   miniz.c v1.15 - public domain deflate/inflate, zlib-subset, ZIP
   reading/writing/appending, PNG writing See "unlicense" statement at the end
   of this file. Rich Geldreich <richgel99@gmail.com>, last updated Oct. 13,
   2013 Implements RFC 1950: http://www.ietf.org/rfc/rfc1950.txt and RFC 1951:
   http://www.ietf.org/rfc/rfc1951.txt

   Most API's defined in miniz.c are optional. For example, to disable the
   archive related functions just define MINIZ_NO_ARCHIVE_APIS, or to get rid of
   all stdio usage define MINIZ_NO_STDIO (see the list below for more macros).

   * Change History
     10/13/13 v1.15 r4 - Interim bugfix release while I work on the next major
   release with Zip64 support (almost there!):
       - Critical fix for the MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY bug
   (thanks kahmyong.moon@hp.com) which could cause locate files to not find
   files. This bug would only have occured in earlier versions if you explicitly
   used this flag, OR if you used miniz_zip_extract_archive_file_to_heap() or
   miniz_zip_add_mem_to_archive_file_in_place() (which used this flag). If you
   can't switch to v1.15 but want to fix this bug, just remove the uses of this
   flag from both helper funcs (and of course don't use the flag).
       - Bugfix in miniz_zip_reader_extract_to_mem_no_alloc() from kymoon when
   pUser_read_buf is not NULL and compressed size is > uncompressed size
       - Fixing miniz_zip_reader_extract_*() funcs so they don't try to extract
   compressed data from directory entries, to account for weird zipfiles which
   contain zero-size compressed data on dir entries. Hopefully this fix won't
   cause any issues on weird zip archives, because it assumes the low 16-bits of
   zip external attributes are DOS attributes (which I believe they always are
   in practice).
       - Fixing miniz_zip_reader_is_file_a_directory() so it doesn't check the
   internal attributes, just the filename and external attributes
       - miniz_zip_reader_init_file() - missing MINIZ_FCLOSE() call if the seek failed
       - Added cmake support for Linux builds which builds all the examples,
   tested with clang v3.3 and gcc v4.6.
       - Clang fix for tdefl_write_image_to_png_file_in_memory() from toffaletti
       - Merged MINIZ_FORCEINLINE fix from hdeanclark
       - Fix <time.h> include before config #ifdef, thanks emil.brink
       - Added tdefl_write_image_to_png_file_in_memory_ex(): supports Y flipping
   (super useful for OpenGL apps), and explicit control over the compression
   level (so you can set it to 1 for real-time compression).
       - Merged in some compiler fixes from paulharris's github repro.
       - Retested this build under Windows (VS 2010, including static analysis),
   tcc  0.9.26, gcc v4.6 and clang v3.3.
       - Added example6.c, which dumps an image of the mandelbrot set to a PNG
   file.
       - Modified example2 to help test the
   MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY flag more.
       - In r3: Bugfix to miniz_zip_writer_add_file() found during merge: Fix
   possible src file fclose() leak if alignment bytes+local header file write
   faiiled
   - In r4: Minor bugfix to miniz_zip_writer_add_from_zip_reader(): Was pushing the
   wrong central dir header offset, appears harmless in this release, but it
   became a problem in the zip64 branch 5/20/12 v1.14 - MinGW32/64 GCC 4.6.1
   compiler fixes: added MINIZ_FORCEINLINE, #include <time.h> (thanks fermtect).
     5/19/12 v1.13 - From jason@cornsyrup.org and kelwert@mtu.edu - Fix
   miniz_crc32() so it doesn't compute the wrong CRC-32's when miniz_ulong is 64-bit.
       - Temporarily/locally slammed in "typedef unsigned long miniz_ulong" and
   re-ran a randomized regression test on ~500k files.
       - Eliminated a bunch of warnings when compiling with GCC 32-bit/64.
       - Ran all examples, miniz.c, and tinfl.c through MSVC 2008's /analyze
   (static analysis) option and fixed all warnings (except for the silly "Use of
   the comma-operator in a tested expression.." analysis warning, which I
   purposely use to work around a MSVC compiler warning).
       - Created 32-bit and 64-bit Codeblocks projects/workspace. Built and
   tested Linux executables. The codeblocks workspace is compatible with
   Linux+Win32/x64.
       - Added miniz_tester solution/project, which is a useful little app
   derived from LZHAM's tester app that I use as part of the regression test.
       - Ran miniz.c and tinfl.c through another series of regression testing on
   ~500,000 files and archives.
       - Modified example5.c so it purposely disables a bunch of high-level
   functionality (MINIZ_NO_STDIO, etc.). (Thanks to corysama for the
   MINIZ_NO_STDIO bug report.)
       - Fix ftell() usage in examples so they exit with an error on files which
   are too large (a limitation of the examples, not miniz itself). 4/12/12 v1.12
   - More comments, added low-level example5.c, fixed a couple minor
   level_and_flags issues in the archive API's. level_and_flags can now be set
   to MINIZ_DEFAULT_COMPRESSION. Thanks to Bruce Dawson <bruced@valvesoftware.com>
   for the feedback/bug report. 5/28/11 v1.11 - Added statement from
   unlicense.org 5/27/11 v1.10 - Substantial compressor optimizations:
      - Level 1 is now ~4x faster than before. The L1 compressor's throughput
   now varies between 70-110MB/sec. on a
      - Core i7 (actual throughput varies depending on the type of data, and x64
   vs. x86).
      - Improved baseline L2-L9 compression perf. Also, greatly improved
   compression perf. issues on some file types.
      - Refactored the compression code for better readability and
   maintainability.
      - Added level 10 compression level (L10 has slightly better ratio than
   level 9, but could have a potentially large drop in throughput on some
   files). 5/15/11 v1.09 - Initial stable release.

   * Low-level Deflate/Inflate implementation notes:

     Compression: Use the "tdefl" API's. The compressor supports raw, static,
   and dynamic blocks, lazy or greedy parsing, match length filtering, RLE-only,
   and Huffman-only streams. It performs and compresses approximately as well as
   zlib.

     Decompression: Use the "tinfl" API's. The entire decompressor is
   implemented as a single function coroutine: see tinfl_decompress(). It
   supports decompression into a 32KB (or larger power of 2) wrapping buffer, or
   into a memory block large enough to hold the entire file.

     The low-level tdefl/tinfl API's do not make any use of dynamic memory
   allocation.

   * zlib-style API notes:

     miniz.c implements a fairly large subset of zlib. There's enough
   functionality present for it to be a drop-in zlib replacement in many apps:
        The z_stream struct, optional memory allocation callbacks
        deflateInit/deflateInit2/deflate/deflateReset/deflateEnd/deflateBound
        inflateInit/inflateInit2/inflate/inflateEnd
        compress, compress2, compressBound, uncompress
        CRC-32, Adler-32 - Using modern, minimal code size, CPU cache friendly
   routines. Supports raw deflate streams or standard zlib streams with adler-32
   checking.

     Limitations:
      The callback API's are not implemented yet. No support for gzip headers or
   zlib static dictionaries. I've tried to closely emulate zlib's various
   flavors of stream flushing and return status codes, but there are no
   guarantees that miniz.c pulls this off perfectly.

   * PNG writing: See the tdefl_write_image_to_png_file_in_memory() function,
   originally written by Alex Evans. Supports 1-4 bytes/pixel images.

   * ZIP archive API notes:

     The ZIP archive API's where designed with simplicity and efficiency in
   mind, with just enough abstraction to get the job done with minimal fuss.
   There are simple API's to retrieve file information, read files from existing
   archives, create new archives, append new files to existing archives, or
   clone archive data from one archive to another. It supports archives located
   in memory or the heap, on disk (using stdio.h), or you can specify custom
   file read/write callbacks.

     - Archive reading: Just call this function to read a single file from a
   disk archive:

      void *miniz_zip_extract_archive_file_to_heap(const char *pZip_filename, const
   char *pArchive_name, size_t *pSize, miniz_uint zip_flags);

     For more complex cases, use the "miniz_zip_reader" functions. Upon opening an
   archive, the entire central directory is located and read as-is into memory,
   and subsequent file access only occurs when reading individual files.

     - Archives file scanning: The simple way is to use this function to scan a
   loaded archive for a specific file:

     int miniz_zip_reader_locate_file(miniz_zip_archive *pZip, const char *pName,
   const char *pComment, miniz_uint flags);

     The locate operation can optionally check file comments too, which (as one
   example) can be used to identify multiple versions of the same file in an
   archive. This function uses a simple linear search through the central
     directory, so it's not very fast.

     Alternately, you can iterate through all the files in an archive (using
   miniz_zip_reader_get_num_files()) and retrieve detailed info on each file by
   calling miniz_zip_reader_file_stat().

     - Archive creation: Use the "miniz_zip_writer" functions. The ZIP writer
   immediately writes compressed file data to disk and builds an exact image of
   the central directory in memory. The central directory image is written all
   at once at the end of the archive file when the archive is finalized.

     The archive writer can optionally align each file's local header and file
   data to any power of 2 alignment, which can be useful when the archive will
   be read from optical media. Also, the writer supports placing arbitrary data
   blobs at the very beginning of ZIP archives. Archives written using either
   feature are still readable by any ZIP tool.

     - Archive appending: The simple way to add a single file to an archive is
   to call this function:

      miniz_bool miniz_zip_add_mem_to_archive_file_in_place(const char *pZip_filename,
   const char *pArchive_name, const void *pBuf, size_t buf_size, const void
   *pComment, miniz_uint16 comment_size, miniz_uint level_and_flags);

     The archive will be created if it doesn't already exist, otherwise it'll be
   appended to. Note the appending is done in-place and is not an atomic
   operation, so if something goes wrong during the operation it's possible the
   archive could be left without a central directory (although the local file
   headers and file data will be fine, so the archive will be recoverable).

     For more complex archive modification scenarios:
     1. The safest way is to use a miniz_zip_reader to read the existing archive,
   cloning only those bits you want to preserve into a new archive using using
   the miniz_zip_writer_add_from_zip_reader() function (which compiles the
     compressed file data as-is). When you're done, delete the old archive and
   rename the newly written archive, and you're done. This is safe but requires
   a bunch of temporary disk space or heap memory.

     2. Or, you can convert an miniz_zip_reader in-place to an miniz_zip_writer using
   miniz_zip_writer_init_from_reader(), append new files as needed, then finalize
   the archive which will write an updated central directory to the original
   archive. (This is basically what miniz_zip_add_mem_to_archive_file_in_place()
   does.) There's a possibility that the archive's central directory could be
   lost with this method if anything goes wrong, though.

     - ZIP archive support limitations:
     No zip64 or spanning support. Extraction functions can only handle
   unencrypted, stored or deflated files. Requires streams capable of seeking.

   * This is a header file library, like stb_image.c. To get only a header file,
   either cut and paste the below header, or create miniz.h, #define
   MINIZ_HEADER_FILE_ONLY, and then include miniz.c from it.

   * Important: For best perf. be sure to customize the below macros for your
   target platform: #define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1 #define
   MINIZ_LITTLE_ENDIAN 1 #define MINIZ_HAS_64BIT_REGISTERS 1

   * On platforms using glibc, Be sure to "#define _LARGEFILE64_SOURCE 1" before
   including miniz.c to ensure miniz uses the 64-bit variants: fopen64(),
   stat64(), etc. Otherwise you won't be able to process large files (i.e.
   32-bit stat() fails for me on files > 0x7FFFFFFF bytes).
*/

#ifndef MINIZ_HEADER_INCLUDED
#define MINIZ_HEADER_INCLUDED

#include <stdint.h>
#include <stdlib.h>

// Defines to completely disable specific portions of miniz.c:
// If all macros here are defined the only functionality remaining will be
// CRC-32, adler-32, tinfl, and tdefl.

// Define MINIZ_NO_STDIO to disable all usage and any functions which rely on
// stdio for file I/O.
//#define MINIZ_NO_STDIO

// If MINIZ_NO_TIME is specified then the ZIP archive functions will not be able
// to get the current time, or get/set file times, and the C run-time funcs that
// get/set times won't be called. The current downside is the times written to
// your archives will be from 1979.
//#define MINIZ_NO_TIME

// Define MINIZ_NO_ARCHIVE_APIS to disable all ZIP archive API's.
//#define MINIZ_NO_ARCHIVE_APIS

// Define MINIZ_NO_ARCHIVE_APIS to disable all writing related ZIP archive
// API's.
//#define MINIZ_NO_ARCHIVE_WRITING_APIS

// Define MINIZ_NO_ZLIB_APIS to remove all ZLIB-style compression/decompression
// API's.
//#define MINIZ_NO_ZLIB_APIS

// Define MINIZ_NO_ZLIB_COMPATIBLE_NAME to disable zlib names, to prevent
// conflicts against stock zlib.
//#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES

// Define MINIZ_NO_MALLOC to disable all calls to malloc, free, and realloc.
// Note if MINIZ_NO_MALLOC is defined then the user must always provide custom
// user alloc/free/realloc callbacks to the zlib and archive API's, and a few
// stand-alone helper API's which don't provide custom user functions (such as
// tdefl_compress_mem_to_heap() and tinfl_decompress_mem_to_heap()) won't work.
// CONFFX_BEGIN - Custom Allocator
#define MINIZ_FORGE_IO
#include "../../../OS/Interfaces/IFileSystem.h"
// CONFFX_END

#if defined(__TINYC__) && (defined(__linux) || defined(__linux__))
// TODO: Work around "error: include file 'sys\utime.h' when compiling with tcc
// on Linux
#define MINIZ_NO_TIME
#endif

#if !defined(MINIZ_NO_TIME) && !defined(MINIZ_NO_ARCHIVE_APIS)
#include <time.h>
#endif

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) ||                \
    defined(__i386) || defined(__i486__) || defined(__i486) ||                 \
    defined(i386) || defined(__ia64__) || defined(__x86_64__)
// MINIZ_X86_OR_X64_CPU is only used to help set the below macros.
#define MINIZ_X86_OR_X64_CPU 1
#endif

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || MINIZ_X86_OR_X64_CPU
// Set MINIZ_LITTLE_ENDIAN to 1 if the processor is little endian.
#define MINIZ_LITTLE_ENDIAN 1
#endif

/* Set MINIZ_USE_UNALIGNED_LOADS_AND_STORES only if not set */
#if !defined(MINIZ_USE_UNALIGNED_LOADS_AND_STORES)
#if MINIZ_X86_OR_X64_CPU
/* Set MINIZ_USE_UNALIGNED_LOADS_AND_STORES to 1 on CPU's that permit efficient
 * integer loads and stores from unaligned addresses. */
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
#define MINIZ_UNALIGNED_USE_MEMCPY
#else
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif
#endif

#if defined(_M_X64) || defined(_WIN64) || defined(__MINGW64__) ||              \
    defined(_LP64) || defined(__LP64__) || defined(__ia64__) ||                \
    defined(__x86_64__)
// Set MINIZ_HAS_64BIT_REGISTERS to 1 if operations on 64-bit integers are
// reasonably fast (and don't involve compiler generated calls to helper
// functions).
#define MINIZ_HAS_64BIT_REGISTERS 1
#endif

#ifdef __APPLE__
#define ftello64 ftello
#define fseeko64 fseeko
#define fopen64 fopen
#define freopen64 freopen

// Darwin OSX
#define MINIZ_PLATFORM 19
#endif

#ifndef MINIZ_PLATFORM
#if defined(_WIN64) || defined(_WIN32) || defined(__WIN32__)
#define MINIZ_PLATFORM 0
#else
// UNIX
#define MINIZ_PLATFORM 3
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ------------------- zlib-style API Definitions.

// For more compatibility with zlib, miniz.c uses unsigned long for some
// parameters/struct members. Beware: miniz_ulong can be either 32 or 64-bits!
typedef unsigned long miniz_ulong;

// miniz_free() internally uses the MINIZ_FREE() macro (which by default calls free()
// unless you've modified the MINIZ_MALLOC macro) to release a block allocated from
// the heap.
void miniz_free(void *p);

#define MINIZ_ADLER32_INIT (1)
// miniz_adler32() returns the initial adler-32 value to use when called with
// ptr==NULL.
miniz_ulong miniz_adler32(miniz_ulong adler, const unsigned char *ptr, size_t buf_len);

#define MINIZ_CRC32_INIT (0)
// miniz_crc32() returns the initial CRC-32 value to use when called with
// ptr==NULL.
miniz_ulong miniz_crc32(miniz_ulong crc, const unsigned char *ptr, size_t buf_len);

// Compression strategies.
enum {
  MINIZ_DEFAULT_STRATEGY = 0,
  MINIZ_FILTERED = 1,
  MINIZ_HUFFMAN_ONLY = 2,
  MINIZ_RLE = 3,
  MINIZ_FIXED = 4
};

/* miniz error codes. Be sure to update miniz_zip_get_error_string() if you add or
 * modify this enum. */
typedef enum {
  MINIZ_ZIP_NO_ERROR = 0,
  MINIZ_ZIP_UNDEFINED_ERROR,
  MINIZ_ZIP_TOO_MANY_FILES,
  MINIZ_ZIP_FILE_TOO_LARGE,
  MINIZ_ZIP_UNSUPPORTED_METHOD,
  MINIZ_ZIP_UNSUPPORTED_ENCRYPTION,
  MINIZ_ZIP_UNSUPPORTED_FEATURE,
  MINIZ_ZIP_FAILED_FINDING_CENTRAL_DIR,
  MINIZ_ZIP_NOT_AN_ARCHIVE,
  MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED,
  MINIZ_ZIP_UNSUPPORTED_MULTIDISK,
  MINIZ_ZIP_DECOMPRESSION_FAILED,
  MINIZ_ZIP_COMPRESSION_FAILED,
  MINIZ_ZIP_UNEXPECTED_DECOMPRESSED_SIZE,
  MINIZ_ZIP_CRC_CHECK_FAILED,
  MINIZ_ZIP_UNSUPPORTED_CDIR_SIZE,
  MINIZ_ZIP_ALLOC_FAILED,
  MINIZ_ZIP_FILE_OPEN_FAILED,
  MINIZ_ZIP_FILE_CREATE_FAILED,
  MINIZ_ZIP_FILE_WRITE_FAILED,
  MINIZ_ZIP_FILE_READ_FAILED,
  MINIZ_ZIP_FILE_CLOSE_FAILED,
  MINIZ_ZIP_FILE_SEEK_FAILED,
  MINIZ_ZIP_FILE_STAT_FAILED,
  MINIZ_ZIP_INVALID_PARAMETER,
  MINIZ_ZIP_INVALID_FILENAME,
  MINIZ_ZIP_BUF_TOO_SMALL,
  MINIZ_ZIP_INTERNAL_ERROR,
  MINIZ_ZIP_FILE_NOT_FOUND,
  MINIZ_ZIP_ARCHIVE_TOO_LARGE,
  MINIZ_ZIP_VALIDATION_FAILED,
  MINIZ_ZIP_WRITE_CALLBACK_FAILED,
  MINIZ_ZIP_TOTAL_ERRORS
} miniz_zip_error;

// Method
#define MINIZ_DEFLATED 8

#ifndef MINIZ_NO_ZLIB_APIS

// Heap allocation callbacks.
// Note that miniz_alloc_func parameter types purpsosely differ from zlib's:
// items/size is size_t, not unsigned long.
typedef void *(*miniz_alloc_func)(void *opaque, size_t items, size_t size);
typedef void (*miniz_free_func)(void *opaque, void *address);
typedef void *(*miniz_realloc_func)(void *opaque, void *address, size_t items,
                                 size_t size);

#define MINIZ_VERSION "9.1.15"
#define MINIZ_VERNUM 0x91F0
#define MINIZ_VER_MAJOR 9
#define MINIZ_VER_MINOR 1
#define MINIZ_VER_REVISION 15
#define MINIZ_VER_SUBREVISION 0

// Flush values. For typical usage you only need MINIZ_NO_FLUSH and MINIZ_FINISH. The
// other values are for advanced use (refer to the zlib docs).
enum {
  MINIZ_NO_FLUSH = 0,
  MINIZ_PARTIAL_FLUSH = 1,
  MINIZ_SYNC_FLUSH = 2,
  MINIZ_FULL_FLUSH = 3,
  MINIZ_FINISH = 4,
  MINIZ_BLOCK = 5
};

// Return status codes. MINIZ_PARAM_ERROR is non-standard.
enum {
  MINIZ_OK = 0,
  MINIZ_STREAM_END = 1,
  MINIZ_NEED_DICT = 2,
  MINIZ_ERRNO = -1,
  MINIZ_STREAM_ERROR = -2,
  MINIZ_DATA_ERROR = -3,
  MINIZ_MEM_ERROR = -4,
  MINIZ_BUF_ERROR = -5,
  MINIZ_VERSION_ERROR = -6,
  MINIZ_PARAM_ERROR = -10000
};

// Compression levels: 0-9 are the standard zlib-style levels, 10 is best
// possible compression (not zlib compatible, and may be very slow),
// MINIZ_DEFAULT_COMPRESSION=MINIZ_DEFAULT_LEVEL.
enum {
  MINIZ_NO_COMPRESSION = 0,
  MINIZ_BEST_SPEED = 1,
  MINIZ_BEST_COMPRESSION = 9,
  MINIZ_UBER_COMPRESSION = 10,
  MINIZ_DEFAULT_LEVEL = 6,
  MINIZ_DEFAULT_COMPRESSION = -1
};

// Window bits
#define MINIZ_DEFAULT_WINDOW_BITS 15

struct miniz_internal_state;

// Compression/decompression stream struct.
typedef struct miniz_stream_s {
  const unsigned char *next_in; // pointer to next byte to read
  unsigned int avail_in;        // number of bytes available at next_in
  miniz_ulong total_in;            // total number of bytes consumed so far

  unsigned char *next_out; // pointer to next byte to write
  unsigned int avail_out;  // number of bytes that can be written to next_out
  miniz_ulong total_out;      // total number of bytes produced so far

  char *msg;                       // error msg (unused)
  struct miniz_internal_state *state; // internal state, allocated by zalloc/zfree

  miniz_alloc_func
      zalloc;         // optional heap allocation function (defaults to malloc)
  miniz_free_func zfree; // optional heap free function (defaults to free)
  void *opaque;       // heap alloc function user pointer

  int data_type;     // data_type (unused)
  miniz_ulong adler;    // adler32 of the source or uncompressed data
  miniz_ulong reserved; // not used
} miniz_stream;

typedef miniz_stream *miniz_streamp;

// Returns the version string of miniz.c.
const char *miniz_version(void);

// miniz_deflateInit() initializes a compressor with default options:
// Parameters:
//  pStream must point to an initialized miniz_stream struct.
//  level must be between [MINIZ_NO_COMPRESSION, MINIZ_BEST_COMPRESSION].
//  level 1 enables a specially optimized compression function that's been
//  optimized purely for performance, not ratio. (This special func. is
//  currently only enabled when MINIZ_USE_UNALIGNED_LOADS_AND_STORES and
//  MINIZ_LITTLE_ENDIAN are defined.)
// Return values:
//  MINIZ_OK on success.
//  MINIZ_STREAM_ERROR if the stream is bogus.
//  MINIZ_PARAM_ERROR if the input parameters are bogus.
//  MINIZ_MEM_ERROR on out of memory.
int miniz_deflateInit(miniz_streamp pStream, int level);

// miniz_deflateInit2() is like miniz_deflate(), except with more control:
// Additional parameters:
//   method must be MINIZ_DEFLATED
//   window_bits must be MINIZ_DEFAULT_WINDOW_BITS (to wrap the deflate stream with
//   zlib header/adler-32 footer) or -MINIZ_DEFAULT_WINDOW_BITS (raw deflate/no
//   header or footer) mem_level must be between [1, 9] (it's checked but
//   ignored by miniz.c)
int miniz_deflateInit2(miniz_streamp pStream, int level, int method, int window_bits,
                    int mem_level, int strategy);

// Quickly resets a compressor without having to reallocate anything. Same as
// calling miniz_deflateEnd() followed by miniz_deflateInit()/miniz_deflateInit2().
int miniz_deflateReset(miniz_streamp pStream);

// miniz_deflate() compresses the input to output, consuming as much of the input
// and producing as much output as possible. Parameters:
//   pStream is the stream to read from and write to. You must initialize/update
//   the next_in, avail_in, next_out, and avail_out members. flush may be
//   MINIZ_NO_FLUSH, MINIZ_PARTIAL_FLUSH/MINIZ_SYNC_FLUSH, MINIZ_FULL_FLUSH, or MINIZ_FINISH.
// Return values:
//   MINIZ_OK on success (when flushing, or if more input is needed but not
//   available, and/or there's more output to be written but the output buffer
//   is full). MINIZ_STREAM_END if all input has been consumed and all output bytes
//   have been written. Don't call miniz_deflate() on the stream anymore.
//   MINIZ_STREAM_ERROR if the stream is bogus.
//   MINIZ_PARAM_ERROR if one of the parameters is invalid.
//   MINIZ_BUF_ERROR if no forward progress is possible because the input and/or
//   output buffers are empty. (Fill up the input buffer or free up some output
//   space and try again.)
int miniz_deflate(miniz_streamp pStream, int flush);

// miniz_deflateEnd() deinitializes a compressor:
// Return values:
//  MINIZ_OK on success.
//  MINIZ_STREAM_ERROR if the stream is bogus.
int miniz_deflateEnd(miniz_streamp pStream);

// miniz_deflateBound() returns a (very) conservative upper bound on the amount of
// data that could be generated by deflate(), assuming flush is set to only
// MINIZ_NO_FLUSH or MINIZ_FINISH.
miniz_ulong miniz_deflateBound(miniz_streamp pStream, miniz_ulong source_len);

// Single-call compression functions miniz_compress() and miniz_compress2():
// Returns MINIZ_OK on success, or one of the error codes from miniz_deflate() on
// failure.
int miniz_compress(unsigned char *pDest, miniz_ulong *pDest_len,
                const unsigned char *pSource, miniz_ulong source_len);
int miniz_compress2(unsigned char *pDest, miniz_ulong *pDest_len,
                 const unsigned char *pSource, miniz_ulong source_len, int level);

// miniz_compressBound() returns a (very) conservative upper bound on the amount of
// data that could be generated by calling miniz_compress().
miniz_ulong miniz_compressBound(miniz_ulong source_len);

// Initializes a decompressor.
int miniz_inflateInit(miniz_streamp pStream);

// miniz_inflateInit2() is like miniz_inflateInit() with an additional option that
// controls the window size and whether or not the stream has been wrapped with
// a zlib header/footer: window_bits must be MINIZ_DEFAULT_WINDOW_BITS (to parse
// zlib header/footer) or -MINIZ_DEFAULT_WINDOW_BITS (raw deflate).
int miniz_inflateInit2(miniz_streamp pStream, int window_bits);

// Decompresses the input stream to the output, consuming only as much of the
// input as needed, and writing as much to the output as possible. Parameters:
//   pStream is the stream to read from and write to. You must initialize/update
//   the next_in, avail_in, next_out, and avail_out members. flush may be
//   MINIZ_NO_FLUSH, MINIZ_SYNC_FLUSH, or MINIZ_FINISH. On the first call, if flush is
//   MINIZ_FINISH it's assumed the input and output buffers are both sized large
//   enough to decompress the entire stream in a single call (this is slightly
//   faster). MINIZ_FINISH implies that there are no more source bytes available
//   beside what's already in the input buffer, and that the output buffer is
//   large enough to hold the rest of the decompressed data.
// Return values:
//   MINIZ_OK on success. Either more input is needed but not available, and/or
//   there's more output to be written but the output buffer is full.
//   MINIZ_STREAM_END if all needed input has been consumed and all output bytes
//   have been written. For zlib streams, the adler-32 of the decompressed data
//   has also been verified. MINIZ_STREAM_ERROR if the stream is bogus.
//   MINIZ_DATA_ERROR if the deflate stream is invalid.
//   MINIZ_PARAM_ERROR if one of the parameters is invalid.
//   MINIZ_BUF_ERROR if no forward progress is possible because the input buffer is
//   empty but the inflater needs more input to continue, or if the output
//   buffer is not large enough. Call miniz_inflate() again with more input data,
//   or with more room in the output buffer (except when using single call
//   decompression, described above).
int miniz_inflate(miniz_streamp pStream, int flush);

// Deinitializes a decompressor.
int miniz_inflateEnd(miniz_streamp pStream);

// Single-call decompression.
// Returns MINIZ_OK on success, or one of the error codes from miniz_inflate() on
// failure.
int miniz_uncompress(unsigned char *pDest, miniz_ulong *pDest_len,
                  const unsigned char *pSource, miniz_ulong source_len);

// Returns a string description of the specified error code, or NULL if the
// error code is invalid.
const char *miniz_error(int err);

// Redefine zlib-compatible names to miniz equivalents, so miniz.c can be used
// as a drop-in replacement for the subset of zlib that miniz.c supports. Define
// MINIZ_NO_ZLIB_COMPATIBLE_NAMES to disable zlib-compatibility if you use zlib
// in the same project.
#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
typedef unsigned char Byte;
typedef unsigned int uInt;
typedef miniz_ulong uLong;
typedef Byte Bytef;
typedef uInt uIntf;
typedef char charf;
typedef int intf;
typedef void *voidpf;
typedef uLong uLongf;
typedef void *voidp;
typedef void *const voidpc;
#define Z_NULL 0
#define Z_NO_FLUSH MINIZ_NO_FLUSH
#define Z_PARTIAL_FLUSH MINIZ_PARTIAL_FLUSH
#define Z_SYNC_FLUSH MINIZ_SYNC_FLUSH
#define Z_FULL_FLUSH MINIZ_FULL_FLUSH
#define Z_FINISH MINIZ_FINISH
#define Z_BLOCK MINIZ_BLOCK
#define Z_OK MINIZ_OK
#define Z_STREAM_END MINIZ_STREAM_END
#define Z_NEED_DICT MINIZ_NEED_DICT
#define Z_ERRNO MINIZ_ERRNO
#define Z_STREAM_ERROR MINIZ_STREAM_ERROR
#define Z_DATA_ERROR MINIZ_DATA_ERROR
#define Z_MEM_ERROR MINIZ_MEM_ERROR
#define Z_BUF_ERROR MINIZ_BUF_ERROR
#define Z_VERSION_ERROR MINIZ_VERSION_ERROR
#define Z_PARAM_ERROR MINIZ_PARAM_ERROR
#define Z_NO_COMPRESSION MINIZ_NO_COMPRESSION
#define Z_BEST_SPEED MINIZ_BEST_SPEED
#define Z_BEST_COMPRESSION MINIZ_BEST_COMPRESSION
#define Z_DEFAULT_COMPRESSION MINIZ_DEFAULT_COMPRESSION
#define Z_DEFAULT_STRATEGY MINIZ_DEFAULT_STRATEGY
#define Z_FILTERED MINIZ_FILTERED
#define Z_HUFFMAN_ONLY MINIZ_HUFFMAN_ONLY
#define Z_RLE MINIZ_RLE
#define Z_FIXED MINIZ_FIXED
#define Z_DEFLATED MINIZ_DEFLATED
#define Z_DEFAULT_WINDOW_BITS MINIZ_DEFAULT_WINDOW_BITS
#define alloc_func miniz_alloc_func
#define free_func miniz_free_func
#define internal_state miniz_internal_state
#define z_stream miniz_stream
#define deflateInit miniz_deflateInit
#define deflateInit2 miniz_deflateInit2
#define deflateReset miniz_deflateReset
#define deflate miniz_deflate
#define deflateEnd miniz_deflateEnd
#define deflateBound miniz_deflateBound
#define compress miniz_compress
#define compress2 miniz_compress2
#define compressBound miniz_compressBound
#define inflateInit miniz_inflateInit
#define inflateInit2 miniz_inflateInit2
#define inflate miniz_inflate
#define inflateEnd miniz_inflateEnd
#define uncompress miniz_uncompress
#define crc32 miniz_crc32
#define adler32 miniz_adler32
#define MAX_WBITS 15
#define MAX_MEM_LEVEL 9
#define zError miniz_error
#define ZLIB_VERSION MINIZ_VERSION
#define ZLIB_VERNUM MINIZ_VERNUM
#define ZLIB_VER_MAJOR MINIZ_VER_MAJOR
#define ZLIB_VER_MINOR MINIZ_VER_MINOR
#define ZLIB_VER_REVISION MINIZ_VER_REVISION
#define ZLIB_VER_SUBREVISION MINIZ_VER_SUBREVISION
#define zlibVersion miniz_version
#define zlib_version miniz_version()
#endif // #ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES

#endif // MINIZ_NO_ZLIB_APIS

// ------------------- Types and macros

typedef unsigned char miniz_uint8;
typedef signed short miniz_int16;
typedef unsigned short miniz_uint16;
typedef unsigned int miniz_uint32;
typedef unsigned int miniz_uint;
typedef long long miniz_int64;
typedef unsigned long long miniz_uint64;
typedef int miniz_bool;

#define MINIZ_FALSE (0)
#define MINIZ_TRUE (1)

// An attempt to work around MSVC's spammy "warning C4127: conditional
// expression is constant" message.
#ifdef _MSC_VER
#define MINIZ_MACRO_END while (0, 0)
#else
#define MINIZ_MACRO_END while (0)
#endif

// ------------------- ZIP archive reading/writing

#ifndef MINIZ_NO_ARCHIVE_APIS

enum {
  MINIZ_ZIP_MAX_IO_BUF_SIZE = 64 * 1024,
  MINIZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE = 260,
  MINIZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE = 256
};

typedef struct {
  miniz_uint32 m_file_index;
  miniz_uint32 m_central_dir_ofs;
  miniz_uint16 m_version_made_by;
  miniz_uint16 m_version_needed;
  miniz_uint16 m_bit_flag;
  miniz_uint16 m_method;
#ifndef MINIZ_NO_TIME
  time_t m_time;
#endif
  miniz_uint32 m_crc32;
  miniz_uint64 m_comp_size;
  miniz_uint64 m_uncomp_size;
  miniz_uint16 m_internal_attr;
  miniz_uint32 m_external_attr;
  miniz_uint64 m_local_header_ofs;
  miniz_uint32 m_comment_size;
  char m_filename[MINIZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  char m_comment[MINIZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE];
} miniz_zip_archive_file_stat;

typedef size_t (*miniz_file_read_func)(void *pOpaque, miniz_uint64 file_ofs,
                                    void *pBuf, size_t n);
typedef size_t (*miniz_file_write_func)(void *pOpaque, miniz_uint64 file_ofs,
                                     const void *pBuf, size_t n);
typedef miniz_bool (*miniz_file_needs_keepalive)(void *pOpaque);

struct miniz_zip_internal_state_tag;
typedef struct miniz_zip_internal_state_tag miniz_zip_internal_state;

typedef enum {
  MINIZ_ZIP_MODE_INVALID = 0,
  MINIZ_ZIP_MODE_READING = 1,
  MINIZ_ZIP_MODE_WRITING = 2,
  MINIZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED = 3
} miniz_zip_mode;

typedef enum {
  MINIZ_ZIP_TYPE_INVALID = 0,
  MINIZ_ZIP_TYPE_USER,
  MINIZ_ZIP_TYPE_MEMORY,
  MINIZ_ZIP_TYPE_HEAP,
  MINIZ_ZIP_TYPE_FILE,
  MINIZ_ZIP_TYPE_CFILE,
  MINIZ_ZIP_TOTAL_TYPES
} miniz_zip_type;

typedef struct {
  miniz_uint64 m_archive_size;
  miniz_uint64 m_central_directory_file_ofs;

  /* We only support up to UINT32_MAX files in zip64 mode. */
  miniz_uint32 m_total_files;
  miniz_zip_mode m_zip_mode;
  miniz_zip_type m_zip_type;
  miniz_zip_error m_last_error;

  miniz_uint64 m_file_offset_alignment;

  miniz_alloc_func m_pAlloc;
  miniz_free_func m_pFree;
  miniz_realloc_func m_pRealloc;
  void *m_pAlloc_opaque;

  miniz_file_read_func m_pRead;
  miniz_file_write_func m_pWrite;
  miniz_file_needs_keepalive m_pNeeds_keepalive;
  void *m_pIO_opaque;

  miniz_zip_internal_state *m_pState;

} miniz_zip_archive;

typedef enum {
  MINIZ_ZIP_FLAG_CASE_SENSITIVE = 0x0100,
  MINIZ_ZIP_FLAG_IGNORE_PATH = 0x0200,
  MINIZ_ZIP_FLAG_COMPRESSED_DATA = 0x0400,
  MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY = 0x0800
} miniz_zip_flags;

// ZIP archive reading

// Inits a ZIP archive reader.
// These functions read and validate the archive's central directory.
miniz_bool miniz_zip_reader_init(miniz_zip_archive *pZip, miniz_uint64 size,
                           miniz_uint32 flags);
miniz_bool miniz_zip_reader_init_mem(miniz_zip_archive *pZip, const void *pMem,
                               size_t size, miniz_uint32 flags);

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_reader_init_file(miniz_zip_archive *pZip, const ResourceDirectory resourceDirectory, const char* fileName,
	const char* filePassword, miniz_uint32 flags);
#endif

// Returns the total number of files in the archive.
miniz_uint miniz_zip_reader_get_num_files(miniz_zip_archive *pZip);

// Returns detailed information about an archive file entry.
miniz_bool miniz_zip_reader_file_stat(miniz_zip_archive *pZip, miniz_uint file_index,
                                miniz_zip_archive_file_stat *pStat);

// Determines if an archive file entry is a directory entry.
miniz_bool miniz_zip_reader_is_file_a_directory(miniz_zip_archive *pZip,
                                          miniz_uint file_index);
miniz_bool miniz_zip_reader_is_file_encrypted(miniz_zip_archive *pZip,
                                        miniz_uint file_index);

// Retrieves the filename of an archive file entry.
// Returns the number of bytes written to pFilename, or if filename_buf_size is
// 0 this function returns the number of bytes needed to fully store the
// filename.
miniz_uint miniz_zip_reader_get_filename(miniz_zip_archive *pZip, miniz_uint file_index,
                                   char *pFilename, miniz_uint filename_buf_size);

// Attempts to locates a file in the archive's central directory.
// Valid flags: MINIZ_ZIP_FLAG_CASE_SENSITIVE, MINIZ_ZIP_FLAG_IGNORE_PATH
// Returns -1 if the file cannot be found.
int miniz_zip_reader_locate_file(miniz_zip_archive *pZip, const char *pName,
                              const char *pComment, miniz_uint flags);

// Extracts a archive file to a memory buffer using no memory allocation.
miniz_bool miniz_zip_reader_extract_to_mem_no_alloc(miniz_zip_archive *pZip,
                                              miniz_uint file_index, void *pBuf,
                                              size_t buf_size, miniz_uint flags,
                                              void *pUser_read_buf,
                                              size_t user_read_buf_size);
miniz_bool miniz_zip_reader_extract_file_to_mem_no_alloc(
    miniz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size,
    miniz_uint flags, void *pUser_read_buf, size_t user_read_buf_size);

// Extracts a archive file to a memory buffer.
miniz_bool miniz_zip_reader_extract_to_mem(miniz_zip_archive *pZip, miniz_uint file_index,
                                     void *pBuf, size_t buf_size,
                                     miniz_uint flags);
miniz_bool miniz_zip_reader_extract_file_to_mem(miniz_zip_archive *pZip,
                                          const char *pFilename, void *pBuf,
                                          size_t buf_size, miniz_uint flags);

// Extracts a archive file to a dynamically allocated heap buffer.
void *miniz_zip_reader_extract_to_heap(miniz_zip_archive *pZip, miniz_uint file_index,
                                    size_t *pSize, miniz_uint flags);
void *miniz_zip_reader_extract_file_to_heap(miniz_zip_archive *pZip,
                                         const char *pFilename, size_t *pSize,
                                         miniz_uint flags);

// Extracts a archive file using a callback function to output the file's data.
miniz_bool miniz_zip_reader_extract_to_callback(miniz_zip_archive *pZip,
                                          miniz_uint file_index,
                                          miniz_file_write_func pCallback,
                                          void *pOpaque, miniz_uint flags);
miniz_bool miniz_zip_reader_extract_file_to_callback(miniz_zip_archive *pZip,
                                               const char *pFilename,
                                               miniz_file_write_func pCallback,
                                               void *pOpaque, miniz_uint flags);

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
// Extracts a archive file to a disk file and sets its last accessed and
// modified times. This function only extracts files, not archive directory
// records.
miniz_bool miniz_zip_reader_extract_to_file(miniz_zip_archive *pZip, miniz_uint file_index,
	const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword,
	miniz_uint flags);
miniz_bool miniz_zip_reader_extract_file_to_file(miniz_zip_archive *pZip,
	const char *pArchive_filename,
	const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword,
	miniz_uint flags);
#endif

// Ends archive reading, freeing all allocations, and closing the input archive
// file if miniz_zip_reader_init_file() was used.
miniz_bool miniz_zip_reader_end(miniz_zip_archive *pZip);

// ZIP archive writing

#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS

// Inits a ZIP archive writer.
miniz_bool miniz_zip_writer_init(miniz_zip_archive *pZip, miniz_uint64 existing_size);
miniz_bool miniz_zip_writer_init_heap(miniz_zip_archive *pZip,
                                size_t size_to_reserve_at_beginning,
                                size_t initial_allocation_size);

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_writer_init_file(miniz_zip_archive *pZip, const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword,
	miniz_uint64 size_to_reserve_at_beginning);
#endif

// CONFFX_CHANGE - Custom File IO
// Converts a ZIP archive reader object into a writer object, to allow efficient
// in-place file appends to occur on an existing archive. For archives opened
// using miniz_zip_reader_init_file, pFilename must be the archive's filename so it
// can be reopened for writing. If the file can't be reopened,
// miniz_zip_reader_end() will be called. For archives opened using
// miniz_zip_reader_init_mem, the memory block must be growable using the realloc
// callback (which defaults to realloc unless you've overridden it). Finally,
// for archives opened using miniz_zip_reader_init, the miniz_zip_archive's user
// provided m_pWrite function cannot be NULL. Note: In-place archive
// modification is not recommended unless you know what you're doing, because if
// execution stops or something goes wrong before the archive is finalized the
// file's central directory will be hosed.
miniz_bool miniz_zip_writer_init_from_reader(miniz_zip_archive *pZip,
                                       const ResourceDirectory resourceDirectory, const char* fileName);

// Adds the contents of a memory buffer to an archive. These functions record
// the current local time into the archive. To add a directory entry, call this
// method with an archive name ending in a forwardslash with empty buffer.
// level_and_flags - compression level (0-10, see MINIZ_BEST_SPEED,
// MINIZ_BEST_COMPRESSION, etc.) logically OR'd with zero or more miniz_zip_flags, or
// just set to MINIZ_DEFAULT_COMPRESSION.
miniz_bool miniz_zip_writer_add_mem(miniz_zip_archive *pZip, const char *pArchive_name,
                              const void *pBuf, size_t buf_size,
                              miniz_uint level_and_flags);
miniz_bool miniz_zip_writer_add_mem_ex(miniz_zip_archive *pZip,
                                 const char *pArchive_name, const void *pBuf,
                                 size_t buf_size, const void *pComment,
                                 miniz_uint16 comment_size,
                                 miniz_uint level_and_flags, miniz_uint64 uncomp_size,
                                 miniz_uint32 uncomp_crc32);

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
// Adds the contents of a disk file to an archive. This function also records
// the disk file's modified time into the archive. level_and_flags - compression
// level (0-10, see MINIZ_BEST_SPEED, MINIZ_BEST_COMPRESSION, etc.) logically OR'd
// with zero or more miniz_zip_flags, or just set to MINIZ_DEFAULT_COMPRESSION.
miniz_bool miniz_zip_writer_add_file(miniz_zip_archive *pZip, const char *pArchive_name,
	const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword, const void *pComment,
	miniz_uint16 comment_size, miniz_uint level_and_flags,
	miniz_uint32 ext_attributes);
#endif

// Adds a file to an archive by fully cloning the data from another archive.
// This function fully clones the source file's compressed data (no
// recompression), along with its full filename, extra data, and comment fields.
miniz_bool miniz_zip_writer_add_from_zip_reader(miniz_zip_archive *pZip,
                                          miniz_zip_archive *pSource_zip,
                                          miniz_uint file_index);

// Finalizes the archive by writing the central directory records followed by
// the end of central directory record. After an archive is finalized, the only
// valid call on the miniz_zip_archive struct is miniz_zip_writer_end(). An archive
// must be manually finalized by calling this function for it to be valid.
miniz_bool miniz_zip_writer_finalize_archive(miniz_zip_archive *pZip);
miniz_bool miniz_zip_writer_finalize_heap_archive(miniz_zip_archive *pZip, void **pBuf,
                                            size_t *pSize);

// Ends archive writing, freeing all allocations, and closing the output file if
// miniz_zip_writer_init_file() was used. Note for the archive to be valid, it must
// have been finalized before ending.
miniz_bool miniz_zip_writer_end(miniz_zip_archive *pZip);

// Misc. high-level helper functions:

// CONFFX_CHANGE - Custom File IO
// miniz_zip_add_mem_to_archive_file_in_place() efficiently (but not atomically)
// appends a memory blob to a ZIP archive. level_and_flags - compression level
// (0-10, see MINIZ_BEST_SPEED, MINIZ_BEST_COMPRESSION, etc.) logically OR'd with zero
// or more miniz_zip_flags, or just set to MINIZ_DEFAULT_COMPRESSION.
miniz_bool miniz_zip_add_mem_to_archive_file_in_place(
	const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword, const char *pArchive_name, const void *pBuf,
	size_t buf_size, const void *pComment, miniz_uint16 comment_size,
	miniz_uint level_and_flags);

// CONFFX_CHANGE - Custom File IO
// Reads a single file from an archive into a heap block.
// Returns NULL on failure.
void *miniz_zip_extract_archive_file_to_heap(const ResourceDirectory resourceDirectory, const char* fileName,
	const char* filePassword,
	const char *pArchive_name,
	size_t *pSize, miniz_uint flags);

#endif // #ifndef MINIZ_NO_ARCHIVE_WRITING_APIS

#endif // #ifndef MINIZ_NO_ARCHIVE_APIS

// ------------------- Low-level Decompression API Definitions

// Decompression flags used by tinfl_decompress().
// TINFL_FLAG_PARSE_ZLIB_HEADER: If set, the input has a valid zlib header and
// ends with an adler32 checksum (it's a valid zlib stream). Otherwise, the
// input is a raw deflate stream. TINFL_FLAG_HAS_MORE_INPUT: If set, there are
// more input bytes available beyond the end of the supplied input buffer. If
// clear, the input buffer contains all remaining input.
// TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF: If set, the output buffer is large
// enough to hold the entire decompressed stream. If clear, the output buffer is
// at least the size of the dictionary (typically 32KB).
// TINFL_FLAG_COMPUTE_ADLER32: Force adler-32 checksum computation of the
// decompressed bytes.
enum {
  TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
  TINFL_FLAG_HAS_MORE_INPUT = 2,
  TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
  TINFL_FLAG_COMPUTE_ADLER32 = 8
};

// High level decompression functions:
// tinfl_decompress_mem_to_heap() decompresses a block in memory to a heap block
// allocated via malloc(). On entry:
//  pSrc_buf, src_buf_len: Pointer and size of the Deflate or zlib source data
//  to decompress.
// On return:
//  Function returns a pointer to the decompressed data, or NULL on failure.
//  *pOut_len will be set to the decompressed data's size, which could be larger
//  than src_buf_len on uncompressible data. The caller must call miniz_free() on
//  the returned block when it's no longer needed.
void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len,
                                   size_t *pOut_len, int flags);

// tinfl_decompress_mem_to_mem() decompresses a block in memory to another block
// in memory. Returns TINFL_DECOMPRESS_MEM_TO_MEM_FAILED on failure, or the
// number of bytes written on success.
#define TINFL_DECOMPRESS_MEM_TO_MEM_FAILED ((size_t)(-1))
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len,
                                   const void *pSrc_buf, size_t src_buf_len,
                                   int flags);

// tinfl_decompress_mem_to_callback() decompresses a block in memory to an
// internal 32KB buffer, and a user provided callback function will be called to
// flush the buffer. Returns 1 on success or 0 on failure.
typedef int (*tinfl_put_buf_func_ptr)(const void *pBuf, int len, void *pUser);
int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size,
                                     tinfl_put_buf_func_ptr pPut_buf_func,
                                     void *pPut_buf_user, int flags);

struct tinfl_decompressor_tag;
typedef struct tinfl_decompressor_tag tinfl_decompressor;

// Max size of LZ dictionary.
#define TINFL_LZ_DICT_SIZE 32768

// Return status.
typedef enum {
  TINFL_STATUS_BAD_PARAM = -3,
  TINFL_STATUS_ADLER32_MISMATCH = -2,
  TINFL_STATUS_FAILED = -1,
  TINFL_STATUS_DONE = 0,
  TINFL_STATUS_NEEDS_MORE_INPUT = 1,
  TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinfl_status;

// Initializes the decompressor to its initial state.
#define tinfl_init(r)                                                          \
  do {                                                                         \
    (r)->m_state = 0;                                                          \
  }                                                                            \
  MINIZ_MACRO_END
#define tinfl_get_adler32(r) (r)->m_check_adler32

// Main low-level decompressor coroutine function. This is the only function
// actually needed for decompression. All the other functions are just
// high-level helpers for improved usability. This is a universal API, i.e. it
// can be used as a building block to build any desired higher level
// decompression API. In the limit case, it can be called once per every byte
// input or output.
tinfl_status tinfl_decompress(tinfl_decompressor *r,
                              const miniz_uint8 *pIn_buf_next,
                              size_t *pIn_buf_size, miniz_uint8 *pOut_buf_start,
                              miniz_uint8 *pOut_buf_next, size_t *pOut_buf_size,
                              const miniz_uint32 decomp_flags);

// Internal/private bits follow.
enum {
  TINFL_MAX_HUFF_TABLES = 3,
  TINFL_MAX_HUFF_SYMBOLS_0 = 288,
  TINFL_MAX_HUFF_SYMBOLS_1 = 32,
  TINFL_MAX_HUFF_SYMBOLS_2 = 19,
  TINFL_FAST_LOOKUP_BITS = 10,
  TINFL_FAST_LOOKUP_SIZE = 1 << TINFL_FAST_LOOKUP_BITS
};

typedef struct {
  miniz_uint8 m_code_size[TINFL_MAX_HUFF_SYMBOLS_0];
  miniz_int16 m_look_up[TINFL_FAST_LOOKUP_SIZE],
      m_tree[TINFL_MAX_HUFF_SYMBOLS_0 * 2];
} tinfl_huff_table;

#if MINIZ_HAS_64BIT_REGISTERS
#define TINFL_USE_64BIT_BITBUF 1
#endif

#if TINFL_USE_64BIT_BITBUF
typedef miniz_uint64 tinfl_bit_buf_t;
#define TINFL_BITBUF_SIZE (64)
#else
typedef miniz_uint32 tinfl_bit_buf_t;
#define TINFL_BITBUF_SIZE (32)
#endif

struct tinfl_decompressor_tag {
  miniz_uint32 m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type,
      m_check_adler32, m_dist, m_counter, m_num_extra,
      m_table_sizes[TINFL_MAX_HUFF_TABLES];
  tinfl_bit_buf_t m_bit_buf;
  size_t m_dist_from_out_buf_start;
  tinfl_huff_table m_tables[TINFL_MAX_HUFF_TABLES];
  miniz_uint8 m_raw_header[4],
      m_len_codes[TINFL_MAX_HUFF_SYMBOLS_0 + TINFL_MAX_HUFF_SYMBOLS_1 + 137];
};

// ------------------- Low-level Compression API Definitions

// Set TDEFL_LESS_MEMORY to 1 to use less memory (compression will be slightly
// slower, and raw/dynamic blocks will be output more frequently).
#define TDEFL_LESS_MEMORY 0

// tdefl_init() compression flags logically OR'd together (low 12 bits contain
// the max. number of probes per dictionary search): TDEFL_DEFAULT_MAX_PROBES:
// The compressor defaults to 128 dictionary probes per dictionary search.
// 0=Huffman only, 1=Huffman+LZ (fastest/crap compression), 4095=Huffman+LZ
// (slowest/best compression).
enum {
  TDEFL_HUFFMAN_ONLY = 0,
  TDEFL_DEFAULT_MAX_PROBES = 128,
  TDEFL_MAX_PROBES_MASK = 0xFFF
};

// TDEFL_WRITE_ZLIB_HEADER: If set, the compressor outputs a zlib header before
// the deflate data, and the Adler-32 of the source data at the end. Otherwise,
// you'll get raw deflate data. TDEFL_COMPUTE_ADLER32: Always compute the
// adler-32 of the input data (even when not writing zlib headers).
// TDEFL_GREEDY_PARSING_FLAG: Set to use faster greedy parsing, instead of more
// efficient lazy parsing. TDEFL_NONDETERMINISTIC_PARSING_FLAG: Enable to
// decrease the compressor's initialization time to the minimum, but the output
// may vary from run to run given the same input (depending on the contents of
// memory). TDEFL_RLE_MATCHES: Only look for RLE matches (matches with a
// distance of 1) TDEFL_FILTER_MATCHES: Discards matches <= 5 chars if enabled.
// TDEFL_FORCE_ALL_STATIC_BLOCKS: Disable usage of optimized Huffman tables.
// TDEFL_FORCE_ALL_RAW_BLOCKS: Only use raw (uncompressed) deflate blocks.
// The low 12 bits are reserved to control the max # of hash probes per
// dictionary lookup (see TDEFL_MAX_PROBES_MASK).
enum {
  TDEFL_WRITE_ZLIB_HEADER = 0x01000,
  TDEFL_COMPUTE_ADLER32 = 0x02000,
  TDEFL_GREEDY_PARSING_FLAG = 0x04000,
  TDEFL_NONDETERMINISTIC_PARSING_FLAG = 0x08000,
  TDEFL_RLE_MATCHES = 0x10000,
  TDEFL_FILTER_MATCHES = 0x20000,
  TDEFL_FORCE_ALL_STATIC_BLOCKS = 0x40000,
  TDEFL_FORCE_ALL_RAW_BLOCKS = 0x80000
};

// High level compression functions:
// tdefl_compress_mem_to_heap() compresses a block in memory to a heap block
// allocated via malloc(). On entry:
//  pSrc_buf, src_buf_len: Pointer and size of source block to compress.
//  flags: The max match finder probes (default is 128) logically OR'd against
//  the above flags. Higher probes are slower but improve compression.
// On return:
//  Function returns a pointer to the compressed data, or NULL on failure.
//  *pOut_len will be set to the compressed data's size, which could be larger
//  than src_buf_len on uncompressible data. The caller must free() the returned
//  block when it's no longer needed.
void *tdefl_compress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len,
                                 size_t *pOut_len, int flags);

// tdefl_compress_mem_to_mem() compresses a block in memory to another block in
// memory. Returns 0 on failure.
size_t tdefl_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len,
                                 const void *pSrc_buf, size_t src_buf_len,
                                 int flags);

// Compresses an image to a compressed PNG file in memory.
// On entry:
//  pImage, w, h, and num_chans describe the image to compress. num_chans may be
//  1, 2, 3, or 4. The image pitch in bytes per scanline will be w*num_chans.
//  The leftmost pixel on the top scanline is stored first in memory. level may
//  range from [0,10], use MINIZ_NO_COMPRESSION, MINIZ_BEST_SPEED,
//  MINIZ_BEST_COMPRESSION, etc. or a decent default is MINIZ_DEFAULT_LEVEL If flip is
//  true, the image will be flipped on the Y axis (useful for OpenGL apps).
// On return:
//  Function returns a pointer to the compressed data, or NULL on failure.
//  *pLen_out will be set to the size of the PNG image file.
//  The caller must miniz_free() the returned heap block (which will typically be
//  larger than *pLen_out) when it's no longer needed.
void *tdefl_write_image_to_png_file_in_memory_ex(const void *pImage, int w,
                                                 int h, int num_chans,
                                                 size_t *pLen_out,
                                                 miniz_uint level, miniz_bool flip);
void *tdefl_write_image_to_png_file_in_memory(const void *pImage, int w, int h,
                                              int num_chans, size_t *pLen_out);

// Output stream interface. The compressor uses this interface to write
// compressed data. It'll typically be called TDEFL_OUT_BUF_SIZE at a time.
typedef miniz_bool (*tdefl_put_buf_func_ptr)(const void *pBuf, int len,
                                          void *pUser);

// tdefl_compress_mem_to_output() compresses a block to an output stream. The
// above helpers use this function internally.
miniz_bool tdefl_compress_mem_to_output(const void *pBuf, size_t buf_len,
                                     tdefl_put_buf_func_ptr pPut_buf_func,
                                     void *pPut_buf_user, int flags);

enum {
  TDEFL_MAX_HUFF_TABLES = 3,
  TDEFL_MAX_HUFF_SYMBOLS_0 = 288,
  TDEFL_MAX_HUFF_SYMBOLS_1 = 32,
  TDEFL_MAX_HUFF_SYMBOLS_2 = 19,
  TDEFL_LZ_DICT_SIZE = 32768,
  TDEFL_LZ_DICT_SIZE_MASK = TDEFL_LZ_DICT_SIZE - 1,
  TDEFL_MIN_MATCH_LEN = 3,
  TDEFL_MAX_MATCH_LEN = 258
};

// TDEFL_OUT_BUF_SIZE MUST be large enough to hold a single entire compressed
// output block (using static/fixed Huffman codes).
#if TDEFL_LESS_MEMORY
enum {
  TDEFL_LZ_CODE_BUF_SIZE = 24 * 1024,
  TDEFL_OUT_BUF_SIZE = (TDEFL_LZ_CODE_BUF_SIZE * 13) / 10,
  TDEFL_MAX_HUFF_SYMBOLS = 288,
  TDEFL_LZ_HASH_BITS = 12,
  TDEFL_LEVEL1_HASH_SIZE_MASK = 4095,
  TDEFL_LZ_HASH_SHIFT = (TDEFL_LZ_HASH_BITS + 2) / 3,
  TDEFL_LZ_HASH_SIZE = 1 << TDEFL_LZ_HASH_BITS
};
#else
enum {
  TDEFL_LZ_CODE_BUF_SIZE = 64 * 1024,
  TDEFL_OUT_BUF_SIZE = (TDEFL_LZ_CODE_BUF_SIZE * 13) / 10,
  TDEFL_MAX_HUFF_SYMBOLS = 288,
  TDEFL_LZ_HASH_BITS = 15,
  TDEFL_LEVEL1_HASH_SIZE_MASK = 4095,
  TDEFL_LZ_HASH_SHIFT = (TDEFL_LZ_HASH_BITS + 2) / 3,
  TDEFL_LZ_HASH_SIZE = 1 << TDEFL_LZ_HASH_BITS
};
#endif

// The low-level tdefl functions below may be used directly if the above helper
// functions aren't flexible enough. The low-level functions don't make any heap
// allocations, unlike the above helper functions.
typedef enum {
  TDEFL_STATUS_BAD_PARAM = -2,
  TDEFL_STATUS_PUT_BUF_FAILED = -1,
  TDEFL_STATUS_OKAY = 0,
  TDEFL_STATUS_DONE = 1,
} tdefl_status;

// Must map to MINIZ_NO_FLUSH, MINIZ_SYNC_FLUSH, etc. enums
typedef enum {
  TDEFL_NO_FLUSH = 0,
  TDEFL_SYNC_FLUSH = 2,
  TDEFL_FULL_FLUSH = 3,
  TDEFL_FINISH = 4
} tdefl_flush;

// tdefl's compression state structure.
typedef struct {
  tdefl_put_buf_func_ptr m_pPut_buf_func;
  void *m_pPut_buf_user;
  miniz_uint m_flags, m_max_probes[2];
  int m_greedy_parsing;
  miniz_uint m_adler32, m_lookahead_pos, m_lookahead_size, m_dict_size;
  miniz_uint8 *m_pLZ_code_buf, *m_pLZ_flags, *m_pOutput_buf, *m_pOutput_buf_end;
  miniz_uint m_num_flags_left, m_total_lz_bytes, m_lz_code_buf_dict_pos, m_bits_in,
      m_bit_buffer;
  miniz_uint m_saved_match_dist, m_saved_match_len, m_saved_lit,
      m_output_flush_ofs, m_output_flush_remaining, m_finished, m_block_index,
      m_wants_to_finish;
  tdefl_status m_prev_return_status;
  const void *m_pIn_buf;
  void *m_pOut_buf;
  size_t *m_pIn_buf_size, *m_pOut_buf_size;
  tdefl_flush m_flush;
  const miniz_uint8 *m_pSrc;
  size_t m_src_buf_left, m_out_buf_ofs;
  miniz_uint8 m_dict[TDEFL_LZ_DICT_SIZE + TDEFL_MAX_MATCH_LEN - 1];
  miniz_uint16 m_huff_count[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
  miniz_uint16 m_huff_codes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
  miniz_uint8 m_huff_code_sizes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
  miniz_uint8 m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE];
  miniz_uint16 m_next[TDEFL_LZ_DICT_SIZE];
  miniz_uint16 m_hash[TDEFL_LZ_HASH_SIZE];
  miniz_uint8 m_output_buf[TDEFL_OUT_BUF_SIZE];
} tdefl_compressor;

// Initializes the compressor.
// There is no corresponding deinit() function because the tdefl API's do not
// dynamically allocate memory. pBut_buf_func: If NULL, output data will be
// supplied to the specified callback. In this case, the user should call the
// tdefl_compress_buffer() API for compression. If pBut_buf_func is NULL the
// user should always call the tdefl_compress() API. flags: See the above enums
// (TDEFL_HUFFMAN_ONLY, TDEFL_WRITE_ZLIB_HEADER, etc.)
tdefl_status tdefl_init(tdefl_compressor *d,
                        tdefl_put_buf_func_ptr pPut_buf_func,
                        void *pPut_buf_user, int flags);

// Compresses a block of data, consuming as much of the specified input buffer
// as possible, and writing as much compressed data to the specified output
// buffer as possible.
tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf,
                            size_t *pIn_buf_size, void *pOut_buf,
                            size_t *pOut_buf_size, tdefl_flush flush);

// tdefl_compress_buffer() is only usable when the tdefl_init() is called with a
// non-NULL tdefl_put_buf_func_ptr. tdefl_compress_buffer() always consumes the
// entire input buffer.
tdefl_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf,
                                   size_t in_buf_size, tdefl_flush flush);

tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d);
miniz_uint32 tdefl_get_adler32(tdefl_compressor *d);

// Can't use tdefl_create_comp_flags_from_zip_params if MINIZ_NO_ZLIB_APIS isn't
// defined, because it uses some of its macros.
#ifndef MINIZ_NO_ZLIB_APIS
// Create tdefl_compress() flags given zlib-style compression parameters.
// level may range from [0,10] (where 10 is absolute max compression, but may be
// much slower on some files) window_bits may be -15 (raw deflate) or 15 (zlib)
// strategy may be either MINIZ_DEFAULT_STRATEGY, MINIZ_FILTERED, MINIZ_HUFFMAN_ONLY,
// MINIZ_RLE, or MINIZ_FIXED
miniz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits,
                                                int strategy);
#endif // #ifndef MINIZ_NO_ZLIB_APIS

#define MINIZ_UINT16_MAX (0xFFFFU)
#define MINIZ_UINT32_MAX (0xFFFFFFFFU)

#ifdef __cplusplus
}
#endif

#endif // MINIZ_HEADER_INCLUDED

// ------------------- End of Header: Implementation follows. (If you only want
// the header, define MINIZ_HEADER_FILE_ONLY.)

#ifndef MINIZ_HEADER_FILE_ONLY

typedef unsigned char miniz_validate_uint16[sizeof(miniz_uint16) == 2 ? 1 : -1];
typedef unsigned char miniz_validate_uint32[sizeof(miniz_uint32) == 4 ? 1 : -1];
typedef unsigned char miniz_validate_uint64[sizeof(miniz_uint64) == 8 ? 1 : -1];

#include <assert.h>
#include <string.h>

#define MINIZ_ASSERT(x) assert(x)

#ifdef MINIZ_NO_MALLOC
#define MINIZ_MALLOC(x) tf_malloc(x)
#define MINIZ_FREE(x) tf_free(x)
#define MINIZ_REALLOC(p, x) tf_realloc(p,x)
#else
#define MINIZ_MALLOC(x) malloc(x)
#define MINIZ_FREE(x) free(x)
#define MINIZ_REALLOC(p, x) realloc(p, x)
#endif

#define MINIZ_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MINIZ_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MINIZ_CLEAR_OBJ(obj) memset(&(obj), 0, sizeof(obj))

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
#define MINIZ_READ_LE16(p) *((const miniz_uint16 *)(p))
#define MINIZ_READ_LE32(p) *((const miniz_uint32 *)(p))
#else
#define MINIZ_READ_LE16(p)                                                        \
  ((miniz_uint32)(((const miniz_uint8 *)(p))[0]) |                                   \
   ((miniz_uint32)(((const miniz_uint8 *)(p))[1]) << 8U))
#define MINIZ_READ_LE32(p)                                                        \
  ((miniz_uint32)(((const miniz_uint8 *)(p))[0]) |                                   \
   ((miniz_uint32)(((const miniz_uint8 *)(p))[1]) << 8U) |                           \
   ((miniz_uint32)(((const miniz_uint8 *)(p))[2]) << 16U) |                          \
   ((miniz_uint32)(((const miniz_uint8 *)(p))[3]) << 24U))
#endif

#define MINIZ_READ_LE64(p)                                                        \
  (((miniz_uint64)MINIZ_READ_LE32(p)) |                                              \
   (((miniz_uint64)MINIZ_READ_LE32((const miniz_uint8 *)(p) + sizeof(miniz_uint32)))       \
    << 32U))

#ifdef _MSC_VER
#define MINIZ_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define MINIZ_FORCEINLINE inline __attribute__((__always_inline__))
#else
#define MINIZ_FORCEINLINE inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ------------------- zlib-style API's

miniz_ulong miniz_adler32(miniz_ulong adler, const unsigned char *ptr, size_t buf_len) {
  miniz_uint32 i, s1 = (miniz_uint32)(adler & 0xffff), s2 = (miniz_uint32)(adler >> 16);
  size_t block_len = buf_len % 5552;
  if (!ptr)
    return MINIZ_ADLER32_INIT;
  while (buf_len) {
    for (i = 0; i + 7 < block_len; i += 8, ptr += 8) {
      s1 += ptr[0]; s2 += s1;
      s1 += ptr[1]; s2 += s1;
      s1 += ptr[2]; s2 += s1;
      s1 += ptr[3]; s2 += s1;
      s1 += ptr[4]; s2 += s1;
      s1 += ptr[5]; s2 += s1;
      s1 += ptr[6]; s2 += s1;
      s1 += ptr[7]; s2 += s1;
    }
    for (; i < block_len; ++i)
    {
        s1 += *ptr++; s2 += s1;
    }
    s1 %= 65521U; s2 %= 65521U;
    buf_len -= block_len;
    block_len = 5552;
  }
  return (s2 << 16) + s1;
}

// Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C
// implementation that balances processor cache usage against speed":
// http://www.geocities.com/malbrain/
miniz_ulong miniz_crc32(miniz_ulong crc, const miniz_uint8 *ptr, size_t buf_len) {
  static const miniz_uint32 s_crc32[16] = {
      0,          0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4,
      0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
      0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};
  miniz_uint32 crcu32 = (miniz_uint32)crc;
  if (!ptr)
    return MINIZ_CRC32_INIT;
  crcu32 = ~crcu32;
  while (buf_len--) {
    miniz_uint8 b = *ptr++;
    crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
    crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)];
  }
  return ~crcu32;
}

void miniz_free(void *p) { MINIZ_FREE(p); }

#ifndef MINIZ_NO_ZLIB_APIS

static void *def_alloc_func(void *opaque, size_t items, size_t size) {
  (void)opaque, (void)items, (void)size;
  return MINIZ_MALLOC(items * size);
}
static void def_free_func(void *opaque, void *address) {
  (void)opaque, (void)address;
  MINIZ_FREE(address);
}
static void *def_realloc_func(void *opaque, void *address, size_t items,
                              size_t size) {
  (void)opaque, (void)address, (void)items, (void)size;
  return MINIZ_REALLOC(address, items * size);
}

const char *miniz_version(void) { return MINIZ_VERSION; }

int miniz_deflateInit(miniz_streamp pStream, int level) {
  return miniz_deflateInit2(pStream, level, MINIZ_DEFLATED, MINIZ_DEFAULT_WINDOW_BITS, 9,
                         MINIZ_DEFAULT_STRATEGY);
}

int miniz_deflateInit2(miniz_streamp pStream, int level, int method, int window_bits,
                    int mem_level, int strategy) {
  tdefl_compressor *pComp;
  miniz_uint comp_flags =
      TDEFL_COMPUTE_ADLER32 |
      tdefl_create_comp_flags_from_zip_params(level, window_bits, strategy);

  if (!pStream)
    return MINIZ_STREAM_ERROR;
  if ((method != MINIZ_DEFLATED) || ((mem_level < 1) || (mem_level > 9)) ||
      ((window_bits != MINIZ_DEFAULT_WINDOW_BITS) &&
       (-window_bits != MINIZ_DEFAULT_WINDOW_BITS)))
    return MINIZ_PARAM_ERROR;

  pStream->data_type = 0;
  pStream->adler = MINIZ_ADLER32_INIT;
  pStream->msg = NULL;
  pStream->reserved = 0;
  pStream->total_in = 0;
  pStream->total_out = 0;
  if (!pStream->zalloc)
    pStream->zalloc = def_alloc_func;
  if (!pStream->zfree)
    pStream->zfree = def_free_func;

  pComp = (tdefl_compressor *)pStream->zalloc(pStream->opaque, 1,
                                              sizeof(tdefl_compressor));
  if (!pComp)
    return MINIZ_MEM_ERROR;

  pStream->state = (struct miniz_internal_state *)pComp;

  if (tdefl_init(pComp, NULL, NULL, comp_flags) != TDEFL_STATUS_OKAY) {
    miniz_deflateEnd(pStream);
    return MINIZ_PARAM_ERROR;
  }

  return MINIZ_OK;
}

int miniz_deflateReset(miniz_streamp pStream) {
  if ((!pStream) || (!pStream->state) || (!pStream->zalloc) ||
      (!pStream->zfree))
    return MINIZ_STREAM_ERROR;
  pStream->total_in = pStream->total_out = 0;
  tdefl_init((tdefl_compressor *)pStream->state, NULL, NULL,
             ((tdefl_compressor *)pStream->state)->m_flags);
  return MINIZ_OK;
}

int miniz_deflate(miniz_streamp pStream, int flush) {
  size_t in_bytes, out_bytes;
  miniz_ulong orig_total_in, orig_total_out;
  int miniz_status = MINIZ_OK;

  if ((!pStream) || (!pStream->state) || (flush < 0) || (flush > MINIZ_FINISH) ||
      (!pStream->next_out))
    return MINIZ_STREAM_ERROR;
  if (!pStream->avail_out)
    return MINIZ_BUF_ERROR;

  if (flush == MINIZ_PARTIAL_FLUSH)
    flush = MINIZ_SYNC_FLUSH;

  if (((tdefl_compressor *)pStream->state)->m_prev_return_status ==
      TDEFL_STATUS_DONE)
    return (flush == MINIZ_FINISH) ? MINIZ_STREAM_END : MINIZ_BUF_ERROR;

  orig_total_in = pStream->total_in;
  orig_total_out = pStream->total_out;
  for (;;) {
    tdefl_status defl_status;
    in_bytes = pStream->avail_in;
    out_bytes = pStream->avail_out;

    defl_status = tdefl_compress((tdefl_compressor *)pStream->state,
                                 pStream->next_in, &in_bytes, pStream->next_out,
                                 &out_bytes, (tdefl_flush)flush);
    pStream->next_in += (miniz_uint)in_bytes;
    pStream->avail_in -= (miniz_uint)in_bytes;
    pStream->total_in += (miniz_uint)in_bytes;
    pStream->adler = tdefl_get_adler32((tdefl_compressor *)pStream->state);

    pStream->next_out += (miniz_uint)out_bytes;
    pStream->avail_out -= (miniz_uint)out_bytes;
    pStream->total_out += (miniz_uint)out_bytes;

    if (defl_status < 0) {
      miniz_status = MINIZ_STREAM_ERROR;
      break;
    } else if (defl_status == TDEFL_STATUS_DONE) {
      miniz_status = MINIZ_STREAM_END;
      break;
    } else if (!pStream->avail_out)
      break;
    else if ((!pStream->avail_in) && (flush != MINIZ_FINISH)) {
      if ((flush) || (pStream->total_in != orig_total_in) ||
          (pStream->total_out != orig_total_out))
        break;
      return MINIZ_BUF_ERROR; // Can't make forward progress without some input.
    }
  }
  return miniz_status;
}

int miniz_deflateEnd(miniz_streamp pStream) {
  if (!pStream)
    return MINIZ_STREAM_ERROR;
  if (pStream->state) {
    pStream->zfree(pStream->opaque, pStream->state);
    pStream->state = NULL;
  }
  return MINIZ_OK;
}

miniz_ulong miniz_deflateBound(miniz_streamp pStream, miniz_ulong source_len) {
  (void)pStream;
  // This is really over conservative. (And lame, but it's actually pretty
  // tricky to compute a true upper bound given the way tdefl's blocking works.)
  return MINIZ_MAX(128 + (source_len * 110) / 100,
                128 + source_len + ((source_len / (31 * 1024)) + 1) * 5);
}

int miniz_compress2(unsigned char *pDest, miniz_ulong *pDest_len,
                 const unsigned char *pSource, miniz_ulong source_len, int level) {
  int status;
  miniz_stream stream;
  memset(&stream, 0, sizeof(stream));

  // In case miniz_ulong is 64-bits (argh I hate longs).
  if ((source_len | *pDest_len) > 0xFFFFFFFFU)
    return MINIZ_PARAM_ERROR;

  stream.next_in = pSource;
  stream.avail_in = (miniz_uint32)source_len;
  stream.next_out = pDest;
  stream.avail_out = (miniz_uint32)*pDest_len;

  status = miniz_deflateInit(&stream, level);
  if (status != MINIZ_OK)
    return status;

  status = miniz_deflate(&stream, MINIZ_FINISH);
  if (status != MINIZ_STREAM_END) {
    miniz_deflateEnd(&stream);
    return (status == MINIZ_OK) ? MINIZ_BUF_ERROR : status;
  }

  *pDest_len = stream.total_out;
  return miniz_deflateEnd(&stream);
}

int miniz_compress(unsigned char *pDest, miniz_ulong *pDest_len,
                const unsigned char *pSource, miniz_ulong source_len) {
  return miniz_compress2(pDest, pDest_len, pSource, source_len,
                      MINIZ_DEFAULT_COMPRESSION);
}

miniz_ulong miniz_compressBound(miniz_ulong source_len) {
  return miniz_deflateBound(NULL, source_len);
}

typedef struct {
  tinfl_decompressor m_decomp;
  miniz_uint m_dict_ofs, m_dict_avail, m_first_call, m_has_flushed;
  int m_window_bits;
  miniz_uint8 m_dict[TINFL_LZ_DICT_SIZE];
  tinfl_status m_last_status;
} inflate_state;

int miniz_inflateInit2(miniz_streamp pStream, int window_bits) {
  inflate_state *pDecomp;
  if (!pStream)
    return MINIZ_STREAM_ERROR;
  if ((window_bits != MINIZ_DEFAULT_WINDOW_BITS) &&
      (-window_bits != MINIZ_DEFAULT_WINDOW_BITS))
    return MINIZ_PARAM_ERROR;

  pStream->data_type = 0;
  pStream->adler = 0;
  pStream->msg = NULL;
  pStream->total_in = 0;
  pStream->total_out = 0;
  pStream->reserved = 0;
  if (!pStream->zalloc)
    pStream->zalloc = def_alloc_func;
  if (!pStream->zfree)
    pStream->zfree = def_free_func;

  pDecomp = (inflate_state *)pStream->zalloc(pStream->opaque, 1,
                                             sizeof(inflate_state));
  if (!pDecomp)
    return MINIZ_MEM_ERROR;

  pStream->state = (struct miniz_internal_state *)pDecomp;

  tinfl_init(&pDecomp->m_decomp);
  pDecomp->m_dict_ofs = 0;
  pDecomp->m_dict_avail = 0;
  pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
  pDecomp->m_first_call = 1;
  pDecomp->m_has_flushed = 0;
  pDecomp->m_window_bits = window_bits;

  return MINIZ_OK;
}

int miniz_inflateInit(miniz_streamp pStream) {
  return miniz_inflateInit2(pStream, MINIZ_DEFAULT_WINDOW_BITS);
}

int miniz_inflate(miniz_streamp pStream, int flush) {
  inflate_state *pState;
  miniz_uint n, first_call, decomp_flags = TINFL_FLAG_COMPUTE_ADLER32;
  size_t in_bytes, out_bytes, orig_avail_in;
  tinfl_status status;

  if ((!pStream) || (!pStream->state))
    return MINIZ_STREAM_ERROR;
  if (flush == MINIZ_PARTIAL_FLUSH)
    flush = MINIZ_SYNC_FLUSH;
  if ((flush) && (flush != MINIZ_SYNC_FLUSH) && (flush != MINIZ_FINISH))
    return MINIZ_STREAM_ERROR;

  pState = (inflate_state *)pStream->state;
  if (pState->m_window_bits > 0)
    decomp_flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
  orig_avail_in = pStream->avail_in;

  first_call = pState->m_first_call;
  pState->m_first_call = 0;
  if (pState->m_last_status < 0)
    return MINIZ_DATA_ERROR;

  if (pState->m_has_flushed && (flush != MINIZ_FINISH))
    return MINIZ_STREAM_ERROR;
  pState->m_has_flushed |= (flush == MINIZ_FINISH);

  if ((flush == MINIZ_FINISH) && (first_call)) {
    // MINIZ_FINISH on the first call implies that the input and output buffers are
    // large enough to hold the entire compressed/decompressed file.
    decomp_flags |= TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
    in_bytes = pStream->avail_in;
    out_bytes = pStream->avail_out;
    status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes,
                              pStream->next_out, pStream->next_out, &out_bytes,
                              decomp_flags);
    pState->m_last_status = status;
    pStream->next_in += (miniz_uint)in_bytes;
    pStream->avail_in -= (miniz_uint)in_bytes;
    pStream->total_in += (miniz_uint)in_bytes;
    pStream->adler = tinfl_get_adler32(&pState->m_decomp);
    pStream->next_out += (miniz_uint)out_bytes;
    pStream->avail_out -= (miniz_uint)out_bytes;
    pStream->total_out += (miniz_uint)out_bytes;

    if (status < 0)
      return MINIZ_DATA_ERROR;
    else if (status != TINFL_STATUS_DONE) {
      pState->m_last_status = TINFL_STATUS_FAILED;
      return MINIZ_BUF_ERROR;
    }
    return MINIZ_STREAM_END;
  }
  // flush != MINIZ_FINISH then we must assume there's more input.
  if (flush != MINIZ_FINISH)
    decomp_flags |= TINFL_FLAG_HAS_MORE_INPUT;

  if (pState->m_dict_avail) {
    n = MINIZ_MIN(pState->m_dict_avail, pStream->avail_out);
    memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
    pStream->next_out += n;
    pStream->avail_out -= n;
    pStream->total_out += n;
    pState->m_dict_avail -= n;
    pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);
    return ((pState->m_last_status == TINFL_STATUS_DONE) &&
            (!pState->m_dict_avail))
               ? MINIZ_STREAM_END
               : MINIZ_OK;
  }

  for (;;) {
    in_bytes = pStream->avail_in;
    out_bytes = TINFL_LZ_DICT_SIZE - pState->m_dict_ofs;

    status = tinfl_decompress(
        &pState->m_decomp, pStream->next_in, &in_bytes, pState->m_dict,
        pState->m_dict + pState->m_dict_ofs, &out_bytes, decomp_flags);
    pState->m_last_status = status;

    pStream->next_in += (miniz_uint)in_bytes;
    pStream->avail_in -= (miniz_uint)in_bytes;
    pStream->total_in += (miniz_uint)in_bytes;
    pStream->adler = tinfl_get_adler32(&pState->m_decomp);

    pState->m_dict_avail = (miniz_uint)out_bytes;

    n = MINIZ_MIN(pState->m_dict_avail, pStream->avail_out);
    memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
    pStream->next_out += n;
    pStream->avail_out -= n;
    pStream->total_out += n;
    pState->m_dict_avail -= n;
    pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);

    if (status < 0)
      return MINIZ_DATA_ERROR; // Stream is corrupted (there could be some
                            // uncompressed data left in the output dictionary -
                            // oh well).
    else if ((status == TINFL_STATUS_NEEDS_MORE_INPUT) && (!orig_avail_in))
      return MINIZ_BUF_ERROR; // Signal caller that we can't make forward progress
                           // without supplying more input or by setting flush
                           // to MINIZ_FINISH.
    else if (flush == MINIZ_FINISH) {
      // The output buffer MUST be large to hold the remaining uncompressed data
      // when flush==MINIZ_FINISH.
      if (status == TINFL_STATUS_DONE)
        return pState->m_dict_avail ? MINIZ_BUF_ERROR : MINIZ_STREAM_END;
      // status here must be TINFL_STATUS_HAS_MORE_OUTPUT, which means there's
      // at least 1 more byte on the way. If there's no more room left in the
      // output buffer then something is wrong.
      else if (!pStream->avail_out)
        return MINIZ_BUF_ERROR;
    } else if ((status == TINFL_STATUS_DONE) || (!pStream->avail_in) ||
               (!pStream->avail_out) || (pState->m_dict_avail))
      break;
  }

  return ((status == TINFL_STATUS_DONE) && (!pState->m_dict_avail))
             ? MINIZ_STREAM_END
             : MINIZ_OK;
}

int miniz_inflateEnd(miniz_streamp pStream) {
  if (!pStream)
    return MINIZ_STREAM_ERROR;
  if (pStream->state) {
    pStream->zfree(pStream->opaque, pStream->state);
    pStream->state = NULL;
  }
  return MINIZ_OK;
}

int miniz_uncompress(unsigned char *pDest, miniz_ulong *pDest_len,
                  const unsigned char *pSource, miniz_ulong source_len) {
  miniz_stream stream;
  int status;
  memset(&stream, 0, sizeof(stream));

  // In case miniz_ulong is 64-bits (argh I hate longs).
  if ((source_len | *pDest_len) > 0xFFFFFFFFU)
    return MINIZ_PARAM_ERROR;

  stream.next_in = pSource;
  stream.avail_in = (miniz_uint32)source_len;
  stream.next_out = pDest;
  stream.avail_out = (miniz_uint32)*pDest_len;

  status = miniz_inflateInit(&stream);
  if (status != MINIZ_OK)
    return status;

  status = miniz_inflate(&stream, MINIZ_FINISH);
  if (status != MINIZ_STREAM_END) {
    miniz_inflateEnd(&stream);
    return ((status == MINIZ_BUF_ERROR) && (!stream.avail_in)) ? MINIZ_DATA_ERROR
                                                            : status;
  }
  *pDest_len = stream.total_out;

  return miniz_inflateEnd(&stream);
}

const char *miniz_error(int err) {
  static struct {
    int m_err;
    const char *m_pDesc;
  } s_error_descs[] = {{MINIZ_OK, ""},
                       {MINIZ_STREAM_END, "stream end"},
                       {MINIZ_NEED_DICT, "need dictionary"},
                       {MINIZ_ERRNO, "file error"},
                       {MINIZ_STREAM_ERROR, "stream error"},
                       {MINIZ_DATA_ERROR, "data error"},
                       {MINIZ_MEM_ERROR, "out of memory"},
                       {MINIZ_BUF_ERROR, "buf error"},
                       {MINIZ_VERSION_ERROR, "version error"},
                       {MINIZ_PARAM_ERROR, "parameter error"}};
  miniz_uint i;
  for (i = 0; i < sizeof(s_error_descs) / sizeof(s_error_descs[0]); ++i)
    if (s_error_descs[i].m_err == err)
      return s_error_descs[i].m_pDesc;
  return NULL;
}

#endif // MINIZ_NO_ZLIB_APIS

// ------------------- Low-level Decompression (completely independent from all
// compression API's)

#define TINFL_MEMCPY(d, s, l) memcpy(d, s, l)
#define TINFL_MEMSET(p, c, l) memset(p, c, l)

#define TINFL_CR_BEGIN                                                         \
  switch (r->m_state) {                                                        \
  case 0:
#define TINFL_CR_RETURN(state_index, result)                                   \
  do {                                                                         \
    status = result;                                                           \
    r->m_state = state_index;                                                  \
    goto common_exit;                                                          \
  case state_index:;                                                           \
  }                                                                            \
  MINIZ_MACRO_END
#define TINFL_CR_RETURN_FOREVER(state_index, result)                           \
  do {                                                                         \
    for (;;) {                                                                 \
      TINFL_CR_RETURN(state_index, result);                                    \
    }                                                                          \
  }                                                                            \
  MINIZ_MACRO_END
#define TINFL_CR_FINISH }

// TODO: If the caller has indicated that there's no more input, and we attempt
// to read beyond the input buf, then something is wrong with the input because
// the inflator never reads ahead more than it needs to. Currently
// TINFL_GET_BYTE() pads the end of the stream with 0's in this scenario.
#define TINFL_GET_BYTE(state_index, c)                                         \
  do {                                                                         \
    if (pIn_buf_cur >= pIn_buf_end) {                                          \
      for (;;) {                                                               \
        if (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) {                        \
          TINFL_CR_RETURN(state_index, TINFL_STATUS_NEEDS_MORE_INPUT);         \
          if (pIn_buf_cur < pIn_buf_end) {                                     \
            c = *pIn_buf_cur++;                                                \
            break;                                                             \
          }                                                                    \
        } else {                                                               \
          c = 0;                                                               \
          break;                                                               \
        }                                                                      \
      }                                                                        \
    } else                                                                     \
      c = *pIn_buf_cur++;                                                      \
  }                                                                            \
  MINIZ_MACRO_END

#define TINFL_NEED_BITS(state_index, n)                                        \
  do {                                                                         \
    miniz_uint c;                                                                 \
    TINFL_GET_BYTE(state_index, c);                                            \
    bit_buf |= (((tinfl_bit_buf_t)c) << num_bits);                             \
    num_bits += 8;                                                             \
  } while (num_bits < (miniz_uint)(n))
#define TINFL_SKIP_BITS(state_index, n)                                        \
  do {                                                                         \
    if (num_bits < (miniz_uint)(n)) {                                             \
      TINFL_NEED_BITS(state_index, n);                                         \
    }                                                                          \
    bit_buf >>= (n);                                                           \
    num_bits -= (n);                                                           \
  }                                                                            \
  MINIZ_MACRO_END
#define TINFL_GET_BITS(state_index, b, n)                                      \
  do {                                                                         \
    if (num_bits < (miniz_uint)(n)) {                                             \
      TINFL_NEED_BITS(state_index, n);                                         \
    }                                                                          \
    b = bit_buf & ((1 << (n)) - 1);                                            \
    bit_buf >>= (n);                                                           \
    num_bits -= (n);                                                           \
  }                                                                            \
  MINIZ_MACRO_END

// TINFL_HUFF_BITBUF_FILL() is only used rarely, when the number of bytes
// remaining in the input buffer falls below 2. It reads just enough bytes from
// the input stream that are needed to decode the next Huffman code (and
// absolutely no more). It works by trying to fully decode a Huffman code by
// using whatever bits are currently present in the bit buffer. If this fails,
// it reads another byte, and tries again until it succeeds or until the bit
// buffer contains >=15 bits (deflate's max. Huffman code size).
#define TINFL_HUFF_BITBUF_FILL(state_index, pHuff)                             \
  do {                                                                         \
    temp = (pHuff)->m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)];         \
    if (temp >= 0) {                                                           \
      code_len = temp >> 9;                                                    \
      if ((code_len) && (num_bits >= code_len))                                \
        break;                                                                 \
    } else if (num_bits > TINFL_FAST_LOOKUP_BITS) {                            \
      code_len = TINFL_FAST_LOOKUP_BITS;                                       \
      do {                                                                     \
        temp = (pHuff)->m_tree[~temp + ((bit_buf >> code_len++) & 1)];         \
      } while ((temp < 0) && (num_bits >= (code_len + 1)));                    \
      if (temp >= 0)                                                           \
        break;                                                                 \
    }                                                                          \
    TINFL_GET_BYTE(state_index, c);                                            \
    bit_buf |= (((tinfl_bit_buf_t)c) << num_bits);                             \
    num_bits += 8;                                                             \
  } while (num_bits < 15);

// TINFL_HUFF_DECODE() decodes the next Huffman coded symbol. It's more complex
// than you would initially expect because the zlib API expects the decompressor
// to never read beyond the final byte of the deflate stream. (In other words,
// when this macro wants to read another byte from the input, it REALLY needs
// another byte in order to fully decode the next Huffman code.) Handling this
// properly is particularly important on raw deflate (non-zlib) streams, which
// aren't followed by a byte aligned adler-32. The slow path is only executed at
// the very end of the input buffer.
#define TINFL_HUFF_DECODE(state_index, sym, pHuff)                             \
  do {                                                                         \
    int temp;                                                                  \
    miniz_uint code_len, c;                                                       \
    if (num_bits < 15) {                                                       \
      if ((pIn_buf_end - pIn_buf_cur) < 2) {                                   \
        TINFL_HUFF_BITBUF_FILL(state_index, pHuff);                            \
      } else {                                                                 \
        bit_buf |= (((tinfl_bit_buf_t)pIn_buf_cur[0]) << num_bits) |           \
                   (((tinfl_bit_buf_t)pIn_buf_cur[1]) << (num_bits + 8));      \
        pIn_buf_cur += 2;                                                      \
        num_bits += 16;                                                        \
      }                                                                        \
    }                                                                          \
    if ((temp = (pHuff)->m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= \
        0)                                                                     \
      code_len = temp >> 9, temp &= 511;                                       \
    else {                                                                     \
      code_len = TINFL_FAST_LOOKUP_BITS;                                       \
      do {                                                                     \
        temp = (pHuff)->m_tree[~temp + ((bit_buf >> code_len++) & 1)];         \
      } while (temp < 0);                                                      \
    }                                                                          \
    sym = temp;                                                                \
    bit_buf >>= code_len;                                                      \
    num_bits -= code_len;                                                      \
  }                                                                            \
  MINIZ_MACRO_END

#if defined(_WIN32)
#pragma warning(push)
#pragma warning(disable : 4334)
#endif

tinfl_status tinfl_decompress(tinfl_decompressor *r,
                              const miniz_uint8 *pIn_buf_next,
                              size_t *pIn_buf_size, miniz_uint8 *pOut_buf_start,
                              miniz_uint8 *pOut_buf_next, size_t *pOut_buf_size,
                              const miniz_uint32 decomp_flags) {
  static const int s_length_base[31] = {
      3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23, 27, 31,
      35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,  0};
  static const int s_length_extra[31] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
                                         1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
                                         4, 4, 5, 5, 5, 5, 0, 0, 0};
  static const int s_dist_base[32] = {
      1,    2,    3,    4,    5,    7,     9,     13,    17,  25,   33,
      49,   65,   97,   129,  193,  257,   385,   513,   769, 1025, 1537,
      2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0,   0};
  static const int s_dist_extra[32] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                       4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                       9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  static const miniz_uint8 s_length_dezigzag[19] = {
      16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  static const int s_min_table_sizes[3] = {257, 1, 4};

  tinfl_status status = TINFL_STATUS_FAILED;
  miniz_uint32 num_bits, dist, counter, num_extra;
  tinfl_bit_buf_t bit_buf;
  const miniz_uint8 *pIn_buf_cur = pIn_buf_next, *const pIn_buf_end =
                                                  pIn_buf_next + *pIn_buf_size;
  miniz_uint8 *pOut_buf_cur = pOut_buf_next, *const pOut_buf_end =
                                              pOut_buf_next + *pOut_buf_size;
  size_t out_buf_size_mask =
             (decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)
                 ? (size_t)-1
                 : ((pOut_buf_next - pOut_buf_start) + *pOut_buf_size) - 1,
         dist_from_out_buf_start;

  // Ensure the output buffer's size is a power of 2, unless the output buffer
  // is large enough to hold the entire output file (in which case it doesn't
  // matter).
  if (((out_buf_size_mask + 1) & out_buf_size_mask) ||
      (pOut_buf_next < pOut_buf_start)) {
    *pIn_buf_size = *pOut_buf_size = 0;
    return TINFL_STATUS_BAD_PARAM;
  }

  num_bits = r->m_num_bits;
  bit_buf = r->m_bit_buf;
  dist = r->m_dist;
  counter = r->m_counter;
  num_extra = r->m_num_extra;
  dist_from_out_buf_start = r->m_dist_from_out_buf_start;
  TINFL_CR_BEGIN

  bit_buf = num_bits = dist = counter = num_extra = r->m_zhdr0 = r->m_zhdr1 = 0;
  r->m_z_adler32 = r->m_check_adler32 = 1;
  if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) {
    TINFL_GET_BYTE(1, r->m_zhdr0);
    TINFL_GET_BYTE(2, r->m_zhdr1);
    counter = (((r->m_zhdr0 * 256 + r->m_zhdr1) % 31 != 0) ||
               (r->m_zhdr1 & 32) || ((r->m_zhdr0 & 15) != 8));
    if (!(decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF))
      counter |= (((1U << (8U + (r->m_zhdr0 >> 4))) > 32768U) ||
                  ((out_buf_size_mask + 1) <
                   (size_t)(1U << (8U + (r->m_zhdr0 >> 4)))));
    if (counter) {
      TINFL_CR_RETURN_FOREVER(36, TINFL_STATUS_FAILED);
    }
  }

  do {
    TINFL_GET_BITS(3, r->m_final, 3);
    r->m_type = r->m_final >> 1;
    if (r->m_type == 0) {
      TINFL_SKIP_BITS(5, num_bits & 7);
      for (counter = 0; counter < 4; ++counter) {
        if (num_bits)
          TINFL_GET_BITS(6, r->m_raw_header[counter], 8);
        else
          TINFL_GET_BYTE(7, r->m_raw_header[counter]);
      }
      if ((counter = (r->m_raw_header[0] | (r->m_raw_header[1] << 8))) !=
          (miniz_uint)(0xFFFF ^
                    (r->m_raw_header[2] | (r->m_raw_header[3] << 8)))) {
        TINFL_CR_RETURN_FOREVER(39, TINFL_STATUS_FAILED);
      }
      while ((counter) && (num_bits)) {
        TINFL_GET_BITS(51, dist, 8);
        while (pOut_buf_cur >= pOut_buf_end) {
          TINFL_CR_RETURN(52, TINFL_STATUS_HAS_MORE_OUTPUT);
        }
        *pOut_buf_cur++ = (miniz_uint8)dist;
        counter--;
      }
      while (counter) {
        size_t n;
        while (pOut_buf_cur >= pOut_buf_end) {
          TINFL_CR_RETURN(9, TINFL_STATUS_HAS_MORE_OUTPUT);
        }
        while (pIn_buf_cur >= pIn_buf_end) {
          if (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) {
            TINFL_CR_RETURN(38, TINFL_STATUS_NEEDS_MORE_INPUT);
          } else {
            TINFL_CR_RETURN_FOREVER(40, TINFL_STATUS_FAILED);
          }
        }
        n = MINIZ_MIN(MINIZ_MIN((size_t)(pOut_buf_end - pOut_buf_cur),
                          (size_t)(pIn_buf_end - pIn_buf_cur)),
                   counter);
        TINFL_MEMCPY(pOut_buf_cur, pIn_buf_cur, n);
        pIn_buf_cur += n;
        pOut_buf_cur += n;
        counter -= (miniz_uint)n;
      }
    } else if (r->m_type == 3) {
      TINFL_CR_RETURN_FOREVER(10, TINFL_STATUS_FAILED);
    } else {
      if (r->m_type == 1) {
        miniz_uint8 *p = r->m_tables[0].m_code_size;
        miniz_uint i;
        r->m_table_sizes[0] = 288;
        r->m_table_sizes[1] = 32;
        TINFL_MEMSET(r->m_tables[1].m_code_size, 5, 32);
        for (i = 0; i <= 143; ++i)
          *p++ = 8;
        for (; i <= 255; ++i)
          *p++ = 9;
        for (; i <= 279; ++i)
          *p++ = 7;
        for (; i <= 287; ++i)
          *p++ = 8;
      } else {
        for (counter = 0; counter < 3; counter++) {
          TINFL_GET_BITS(11, r->m_table_sizes[counter], "\05\05\04"[counter]);
          r->m_table_sizes[counter] += s_min_table_sizes[counter];
        }
        MINIZ_CLEAR_OBJ(r->m_tables[2].m_code_size);
        for (counter = 0; counter < r->m_table_sizes[2]; counter++) {
          miniz_uint s;
          TINFL_GET_BITS(14, s, 3);
          r->m_tables[2].m_code_size[s_length_dezigzag[counter]] = (miniz_uint8)s;
        }
        r->m_table_sizes[2] = 19;
      }
      for (; (int)r->m_type >= 0; r->m_type--) {
        int tree_next, tree_cur;
        tinfl_huff_table *pTable;
        miniz_uint i, j, used_syms, total, sym_index, next_code[17],
            total_syms[16];
        pTable = &r->m_tables[r->m_type];
        MINIZ_CLEAR_OBJ(total_syms);
        MINIZ_CLEAR_OBJ(pTable->m_look_up);
        MINIZ_CLEAR_OBJ(pTable->m_tree);
        for (i = 0; i < r->m_table_sizes[r->m_type]; ++i)
          total_syms[pTable->m_code_size[i]]++;
        used_syms = 0; total = 0;
        next_code[0] = next_code[1] = 0;
        for (i = 1; i <= 15; ++i) {
          used_syms += total_syms[i];
          next_code[i + 1] = (total = ((total + total_syms[i]) << 1));
        }
        if ((65536 != total) && (used_syms > 1)) {
          TINFL_CR_RETURN_FOREVER(35, TINFL_STATUS_FAILED);
        }
        for (tree_next = -1, sym_index = 0;
             sym_index < r->m_table_sizes[r->m_type]; ++sym_index) {
          miniz_uint rev_code = 0, l, cur_code,
                  code_size = pTable->m_code_size[sym_index];
          if (!code_size)
            continue;
          cur_code = next_code[code_size]++;
          for (l = code_size; l > 0; l--, cur_code >>= 1)
            rev_code = (rev_code << 1) | (cur_code & 1);
          if (code_size <= TINFL_FAST_LOOKUP_BITS) {
            miniz_int16 k = (miniz_int16)((code_size << 9) | sym_index);
            while (rev_code < TINFL_FAST_LOOKUP_SIZE) {
              pTable->m_look_up[rev_code] = k;
              rev_code += (1 << code_size);
            }
            continue;
          }
          if (0 ==
              (tree_cur = pTable->m_look_up[rev_code &
                                            (TINFL_FAST_LOOKUP_SIZE - 1)])) {
            pTable->m_look_up[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)] =
                (miniz_int16)tree_next;
            tree_cur = tree_next;
            tree_next -= 2;
          }
          rev_code >>= (TINFL_FAST_LOOKUP_BITS - 1);
          for (j = code_size; j > (TINFL_FAST_LOOKUP_BITS + 1); j--) {
            tree_cur -= ((rev_code >>= 1) & 1);
            if (!pTable->m_tree[-tree_cur - 1]) {
              pTable->m_tree[-tree_cur - 1] = (miniz_int16)tree_next;
              tree_cur = tree_next;
              tree_next -= 2;
            } else
              tree_cur = pTable->m_tree[-tree_cur - 1];
          }
          tree_cur -= ((rev_code >>= 1) & 1);
          pTable->m_tree[-tree_cur - 1] = (miniz_int16)sym_index;
        }
        if (r->m_type == 2) {
          for (counter = 0;
               counter < (r->m_table_sizes[0] + r->m_table_sizes[1]);) {
            miniz_uint s;
            TINFL_HUFF_DECODE(16, dist, &r->m_tables[2]);
            if (dist < 16) {
              r->m_len_codes[counter++] = (miniz_uint8)dist;
              continue;
            }
            if ((dist == 16) && (!counter)) {
              TINFL_CR_RETURN_FOREVER(17, TINFL_STATUS_FAILED);
            }
            num_extra = "\02\03\07"[dist - 16];
            TINFL_GET_BITS(18, s, num_extra);
            s += "\03\03\013"[dist - 16];
            TINFL_MEMSET(r->m_len_codes + counter,
                         (dist == 16) ? r->m_len_codes[counter - 1] : 0, s);
            counter += s;
          }
          if ((r->m_table_sizes[0] + r->m_table_sizes[1]) != counter) {
            TINFL_CR_RETURN_FOREVER(21, TINFL_STATUS_FAILED);
          }
          TINFL_MEMCPY(r->m_tables[0].m_code_size, r->m_len_codes,
                       r->m_table_sizes[0]);
          TINFL_MEMCPY(r->m_tables[1].m_code_size,
                       r->m_len_codes + r->m_table_sizes[0],
                       r->m_table_sizes[1]);
        }
      }
      for (;;) {
        miniz_uint8 *pSrc;
        for (;;) {
          if (((pIn_buf_end - pIn_buf_cur) < 4) ||
              ((pOut_buf_end - pOut_buf_cur) < 2)) {
            TINFL_HUFF_DECODE(23, counter, &r->m_tables[0]);
            if (counter >= 256)
              break;
            while (pOut_buf_cur >= pOut_buf_end) {
              TINFL_CR_RETURN(24, TINFL_STATUS_HAS_MORE_OUTPUT);
            }
            *pOut_buf_cur++ = (miniz_uint8)counter;
          } else {
            int sym2;
            miniz_uint code_len;
#if TINFL_USE_64BIT_BITBUF
            if (num_bits < 30) {
              bit_buf |=
                  (((tinfl_bit_buf_t)MINIZ_READ_LE32(pIn_buf_cur)) << num_bits);
              pIn_buf_cur += 4;
              num_bits += 32;
            }
#else
            if (num_bits < 15) {
              bit_buf |=
                  (((tinfl_bit_buf_t)MINIZ_READ_LE16(pIn_buf_cur)) << num_bits);
              pIn_buf_cur += 2;
              num_bits += 16;
            }
#endif
            if ((sym2 =
                     r->m_tables[0]
                         .m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >=
                0)
              code_len = sym2 >> 9;
            else {
              code_len = TINFL_FAST_LOOKUP_BITS;
              do {
                sym2 = r->m_tables[0]
                           .m_tree[~sym2 + ((bit_buf >> code_len++) & 1)];
              } while (sym2 < 0);
            }
            counter = sym2;
            bit_buf >>= code_len;
            num_bits -= code_len;
            if (counter & 256)
              break;

#if !TINFL_USE_64BIT_BITBUF
            if (num_bits < 15) {
              bit_buf |=
                  (((tinfl_bit_buf_t)MINIZ_READ_LE16(pIn_buf_cur)) << num_bits);
              pIn_buf_cur += 2;
              num_bits += 16;
            }
#endif
            if ((sym2 =
                     r->m_tables[0]
                         .m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >=
                0)
              code_len = sym2 >> 9;
            else {
              code_len = TINFL_FAST_LOOKUP_BITS;
              do {
                sym2 = r->m_tables[0]
                           .m_tree[~sym2 + ((bit_buf >> code_len++) & 1)];
              } while (sym2 < 0);
            }
            bit_buf >>= code_len;
            num_bits -= code_len;

            pOut_buf_cur[0] = (miniz_uint8)counter;
            if (sym2 & 256) {
              pOut_buf_cur++;
              counter = sym2;
              break;
            }
            pOut_buf_cur[1] = (miniz_uint8)sym2;
            pOut_buf_cur += 2;
          }
        }
        if ((counter &= 511) == 256)
          break;

        num_extra = s_length_extra[counter - 257];
        counter = s_length_base[counter - 257];
        if (num_extra) {
          miniz_uint extra_bits;
          TINFL_GET_BITS(25, extra_bits, num_extra);
          counter += extra_bits;
        }

        TINFL_HUFF_DECODE(26, dist, &r->m_tables[1]);
        num_extra = s_dist_extra[dist];
        dist = s_dist_base[dist];
        if (num_extra) {
          miniz_uint extra_bits;
          TINFL_GET_BITS(27, extra_bits, num_extra);
          dist += extra_bits;
        }

        dist_from_out_buf_start = pOut_buf_cur - pOut_buf_start;
        if ((dist > dist_from_out_buf_start) &&
            (decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) {
          TINFL_CR_RETURN_FOREVER(37, TINFL_STATUS_FAILED);
        }

        pSrc = pOut_buf_start +
               ((dist_from_out_buf_start - dist) & out_buf_size_mask);

        if ((MINIZ_MAX(pOut_buf_cur, pSrc) + counter) > pOut_buf_end) {
          while (counter--) {
            while (pOut_buf_cur >= pOut_buf_end) {
              TINFL_CR_RETURN(53, TINFL_STATUS_HAS_MORE_OUTPUT);
            }
            *pOut_buf_cur++ =
                pOut_buf_start[(dist_from_out_buf_start++ - dist) &
                               out_buf_size_mask];
          }
          continue;
        }
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
        else if ((counter >= 9) && (counter <= dist)) {
          const miniz_uint8 *pSrc_end = pSrc + (counter & ~7);
          do {
            ((miniz_uint32 *)pOut_buf_cur)[0] = ((const miniz_uint32 *)pSrc)[0];
            ((miniz_uint32 *)pOut_buf_cur)[1] = ((const miniz_uint32 *)pSrc)[1];
            pOut_buf_cur += 8;
          } while ((pSrc += 8) < pSrc_end);
          if ((counter &= 7) < 3) {
            if (counter) {
              pOut_buf_cur[0] = pSrc[0];
              if (counter > 1)
                pOut_buf_cur[1] = pSrc[1];
              pOut_buf_cur += counter;
            }
            continue;
          }
        }
#endif
        do {
          pOut_buf_cur[0] = pSrc[0];
          pOut_buf_cur[1] = pSrc[1];
          pOut_buf_cur[2] = pSrc[2];
          pOut_buf_cur += 3;
          pSrc += 3;
        } while ((int)(counter -= 3) > 2);
        if ((int)counter > 0) {
          pOut_buf_cur[0] = pSrc[0];
          if ((int)counter > 1)
            pOut_buf_cur[1] = pSrc[1];
          pOut_buf_cur += counter;
        }
      }
    }
  } while (!(r->m_final & 1));
  if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) {
    TINFL_SKIP_BITS(32, num_bits & 7);
    for (counter = 0; counter < 4; ++counter) {
      miniz_uint s;
      if (num_bits)
        TINFL_GET_BITS(41, s, 8);
      else
        TINFL_GET_BYTE(42, s);
      r->m_z_adler32 = (r->m_z_adler32 << 8) | s;
    }
  }
  TINFL_CR_RETURN_FOREVER(34, TINFL_STATUS_DONE);
  TINFL_CR_FINISH

common_exit:
  r->m_num_bits = num_bits;
  r->m_bit_buf = bit_buf;
  r->m_dist = dist;
  r->m_counter = counter;
  r->m_num_extra = num_extra;
  r->m_dist_from_out_buf_start = dist_from_out_buf_start;
  *pIn_buf_size = pIn_buf_cur - pIn_buf_next;
  *pOut_buf_size = pOut_buf_cur - pOut_buf_next;
  if ((decomp_flags &
       (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_COMPUTE_ADLER32)) &&
      (status >= 0)) {
    const miniz_uint8 *ptr = pOut_buf_next;
    size_t buf_len = *pOut_buf_size;
    miniz_uint32 i, s1 = r->m_check_adler32 & 0xffff,
                 s2 = r->m_check_adler32 >> 16;
    size_t block_len = buf_len % 5552;
    while (buf_len) {
      for (i = 0; i + 7 < block_len; i += 8, ptr += 8) {
        s1 += ptr[0]; s2 += s1;
        s1 += ptr[1]; s2 += s1;
        s1 += ptr[2]; s2 += s1;
        s1 += ptr[3]; s2 += s1;
        s1 += ptr[4]; s2 += s1;
        s1 += ptr[5]; s2 += s1;
        s1 += ptr[6]; s2 += s1;
        s1 += ptr[7]; s2 += s1;
      }
      for (; i < block_len; ++i)
      {
        s1 += *ptr++; s2 += s1;
      }
      s1 %= 65521U; s2 %= 65521U;
      buf_len -= block_len;
      block_len = 5552;
    }
    r->m_check_adler32 = (s2 << 16) + s1;
    if ((status == TINFL_STATUS_DONE) &&
        (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) &&
        (r->m_check_adler32 != r->m_z_adler32))
      status = TINFL_STATUS_ADLER32_MISMATCH;
  }
  return status;
}

#if defined(_WIN32)
#pragma warning(pop)
#endif

// Higher level helper functions.
void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len,
                                   size_t *pOut_len, int flags) {
  tinfl_decompressor decomp;
  void *pBuf = NULL, *pNew_buf;
  size_t src_buf_ofs = 0, out_buf_capacity = 0;
  *pOut_len = 0;
  tinfl_init(&decomp);
  for (;;) {
    size_t src_buf_size = src_buf_len - src_buf_ofs,
           dst_buf_size = out_buf_capacity - *pOut_len, new_out_buf_capacity;
    tinfl_status status = tinfl_decompress(
        &decomp, (const miniz_uint8 *)pSrc_buf + src_buf_ofs, &src_buf_size,
        (miniz_uint8 *)pBuf, pBuf ? (miniz_uint8 *)pBuf + *pOut_len : NULL,
        &dst_buf_size,
        (flags & ~TINFL_FLAG_HAS_MORE_INPUT) |
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if ((status < 0) || (status == TINFL_STATUS_NEEDS_MORE_INPUT)) {
      MINIZ_FREE(pBuf);
      *pOut_len = 0;
      return NULL;
    }
    src_buf_ofs += src_buf_size;
    *pOut_len += dst_buf_size;
    if (status == TINFL_STATUS_DONE)
      break;
    new_out_buf_capacity = out_buf_capacity * 2;
    if (new_out_buf_capacity < 128)
      new_out_buf_capacity = 128;
    pNew_buf = MINIZ_REALLOC(pBuf, new_out_buf_capacity);
    if (!pNew_buf) {
      MINIZ_FREE(pBuf);
      *pOut_len = 0;
      return NULL;
    }
    pBuf = pNew_buf;
    out_buf_capacity = new_out_buf_capacity;
  }
  return pBuf;
}

size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len,
                                   const void *pSrc_buf, size_t src_buf_len,
                                   int flags) {
  tinfl_decompressor decomp;
  tinfl_status status;
  tinfl_init(&decomp);
  status =
      tinfl_decompress(&decomp, (const miniz_uint8 *)pSrc_buf, &src_buf_len,
                       (miniz_uint8 *)pOut_buf, (miniz_uint8 *)pOut_buf, &out_buf_len,
                       (flags & ~TINFL_FLAG_HAS_MORE_INPUT) |
                           TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  return (status != TINFL_STATUS_DONE) ? TINFL_DECOMPRESS_MEM_TO_MEM_FAILED
                                       : out_buf_len;
}

int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size,
                                     tinfl_put_buf_func_ptr pPut_buf_func,
                                     void *pPut_buf_user, int flags) {
  int result = 0;
  tinfl_decompressor decomp;
  miniz_uint8 *pDict = (miniz_uint8 *)MINIZ_MALLOC(TINFL_LZ_DICT_SIZE);
  size_t in_buf_ofs = 0, dict_ofs = 0;
  if (!pDict)
    return TINFL_STATUS_FAILED;
  tinfl_init(&decomp);
  for (;;) {
    size_t in_buf_size = *pIn_buf_size - in_buf_ofs,
           dst_buf_size = TINFL_LZ_DICT_SIZE - dict_ofs;
    tinfl_status status =
        tinfl_decompress(&decomp, (const miniz_uint8 *)pIn_buf + in_buf_ofs,
                         &in_buf_size, pDict, pDict + dict_ofs, &dst_buf_size,
                         (flags & ~(TINFL_FLAG_HAS_MORE_INPUT |
                                    TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)));
    in_buf_ofs += in_buf_size;
    if ((dst_buf_size) &&
        (!(*pPut_buf_func)(pDict + dict_ofs, (int)dst_buf_size, pPut_buf_user)))
      break;
    if (status != TINFL_STATUS_HAS_MORE_OUTPUT) {
      result = (status == TINFL_STATUS_DONE);
      break;
    }
    dict_ofs = (dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1);
  }
  MINIZ_FREE(pDict);
  *pIn_buf_size = in_buf_ofs;
  return result;
}

// ------------------- Low-level Compression (independent from all decompression
// API's)

// Purposely making these tables static for faster init and thread safety.
static const miniz_uint16 s_tdefl_len_sym[256] = {
    257, 258, 259, 260, 261, 262, 263, 264, 265, 265, 266, 266, 267, 267, 268,
    268, 269, 269, 269, 269, 270, 270, 270, 270, 271, 271, 271, 271, 272, 272,
    272, 272, 273, 273, 273, 273, 273, 273, 273, 273, 274, 274, 274, 274, 274,
    274, 274, 274, 275, 275, 275, 275, 275, 275, 275, 275, 276, 276, 276, 276,
    276, 276, 276, 276, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277,
    277, 277, 277, 277, 277, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 279, 279, 279, 279, 279, 279, 279, 279, 279,
    279, 279, 279, 279, 279, 279, 279, 280, 280, 280, 280, 280, 280, 280, 280,
    280, 280, 280, 280, 280, 280, 280, 280, 281, 281, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 283, 283, 283,
    283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283,
    283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 284,
    284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284,
    284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284,
    285};

static const miniz_uint8 s_tdefl_len_extra[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0};

static const miniz_uint8 s_tdefl_small_dist_sym[512] = {
    0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,
    8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17};

static const miniz_uint8 s_tdefl_small_dist_extra[512] = {
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static const miniz_uint8 s_tdefl_large_dist_sym[128] = {
    0,  0,  18, 19, 20, 20, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 24,
    24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26,
    26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27,
    27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
    29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29};

static const miniz_uint8 s_tdefl_large_dist_extra[128] = {
    0,  0,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13};

// Radix sorts tdefl_sym_freq[] array by 16-bit key m_key. Returns ptr to sorted
// values.
typedef struct {
  miniz_uint16 m_key, m_sym_index;
} tdefl_sym_freq;
static tdefl_sym_freq *tdefl_radix_sort_syms(miniz_uint num_syms,
                                             tdefl_sym_freq *pSyms0,
                                             tdefl_sym_freq *pSyms1) {
  miniz_uint32 total_passes = 2, pass_shift, pass, i, hist[256 * 2];
  tdefl_sym_freq *pCur_syms = pSyms0, *pNew_syms = pSyms1;
  MINIZ_CLEAR_OBJ(hist);
  for (i = 0; i < num_syms; i++) {
    miniz_uint freq = pSyms0[i].m_key;
    hist[freq & 0xFF]++;
    hist[256 + ((freq >> 8) & 0xFF)]++;
  }
  while ((total_passes > 1) && (num_syms == hist[(total_passes - 1) * 256]))
    total_passes--;
  for (pass_shift = 0, pass = 0; pass < total_passes; pass++, pass_shift += 8) {
    const miniz_uint32 *pHist = &hist[pass << 8];
    miniz_uint offsets[256], cur_ofs = 0;
    for (i = 0; i < 256; i++) {
      offsets[i] = cur_ofs;
      cur_ofs += pHist[i];
    }
    for (i = 0; i < num_syms; i++)
      pNew_syms[offsets[(pCur_syms[i].m_key >> pass_shift) & 0xFF]++] =
          pCur_syms[i];
    {
      tdefl_sym_freq *t = pCur_syms;
      pCur_syms = pNew_syms;
      pNew_syms = t;
    }
  }
  return pCur_syms;
}

// tdefl_calculate_minimum_redundancy() originally written by: Alistair Moffat,
// alistair@cs.mu.oz.au, Jyrki Katajainen, jyrki@diku.dk, November 1996.
static void tdefl_calculate_minimum_redundancy(tdefl_sym_freq *A, int n) {
  int root, leaf, next, avbl, used, dpth;
  if (n == 0)
    return;
  else if (n == 1) {
    A[0].m_key = 1;
    return;
  }
  A[0].m_key += A[1].m_key;
  root = 0;
  leaf = 2;
  for (next = 1; next < n - 1; next++) {
    if (leaf >= n || A[root].m_key < A[leaf].m_key) {
      A[next].m_key = A[root].m_key;
      A[root++].m_key = (miniz_uint16)next;
    } else
      A[next].m_key = A[leaf++].m_key;
    if (leaf >= n || (root < next && A[root].m_key < A[leaf].m_key)) {
      A[next].m_key = (miniz_uint16)(A[next].m_key + A[root].m_key);
      A[root++].m_key = (miniz_uint16)next;
    } else
      A[next].m_key = (miniz_uint16)(A[next].m_key + A[leaf++].m_key);
  }
  A[n - 2].m_key = 0;
  for (next = n - 3; next >= 0; next--)
    A[next].m_key = A[A[next].m_key].m_key + 1;
  avbl = 1;
  used = dpth = 0;
  root = n - 2;
  next = n - 1;
  while (avbl > 0) {
    while (root >= 0 && (int)A[root].m_key == dpth) {
      used++;
      root--;
    }
    while (avbl > used) {
      A[next--].m_key = (miniz_uint16)(dpth);
      avbl--;
    }
    avbl = 2 * used;
    dpth++;
    used = 0;
  }
}

// Limits canonical Huffman code table's max code size.
enum { TDEFL_MAX_SUPPORTED_HUFF_CODESIZE = 32 };
static void tdefl_huffman_enforce_max_code_size(int *pNum_codes,
                                                int code_list_len,
                                                int max_code_size) {
  int i;
  miniz_uint32 total = 0;
  if (code_list_len <= 1)
    return;
  for (i = max_code_size + 1; i <= TDEFL_MAX_SUPPORTED_HUFF_CODESIZE; i++)
    pNum_codes[max_code_size] += pNum_codes[i];
  for (i = max_code_size; i > 0; i--)
    total += (((miniz_uint32)pNum_codes[i]) << (max_code_size - i));
  while (total != (1UL << max_code_size)) {
    pNum_codes[max_code_size]--;
    for (i = max_code_size - 1; i > 0; i--)
      if (pNum_codes[i]) {
        pNum_codes[i]--;
        pNum_codes[i + 1] += 2;
        break;
      }
    total--;
  }
}

static void tdefl_optimize_huffman_table(tdefl_compressor *d, int table_num,
                                         int table_len, int code_size_limit,
                                         int static_table) {
  int i, j, l, num_codes[1 + TDEFL_MAX_SUPPORTED_HUFF_CODESIZE];
  miniz_uint next_code[TDEFL_MAX_SUPPORTED_HUFF_CODESIZE + 1];
  MINIZ_CLEAR_OBJ(num_codes);
  if (static_table) {
    for (i = 0; i < table_len; i++)
      num_codes[d->m_huff_code_sizes[table_num][i]]++;
  } else {
    tdefl_sym_freq syms0[TDEFL_MAX_HUFF_SYMBOLS], syms1[TDEFL_MAX_HUFF_SYMBOLS],
        *pSyms;
    int num_used_syms = 0;
    const miniz_uint16 *pSym_count = &d->m_huff_count[table_num][0];
    for (i = 0; i < table_len; i++)
      if (pSym_count[i]) {
        syms0[num_used_syms].m_key = (miniz_uint16)pSym_count[i];
        syms0[num_used_syms++].m_sym_index = (miniz_uint16)i;
      }

    pSyms = tdefl_radix_sort_syms(num_used_syms, syms0, syms1);
    tdefl_calculate_minimum_redundancy(pSyms, num_used_syms);

    for (i = 0; i < num_used_syms; i++)
      num_codes[pSyms[i].m_key]++;

    tdefl_huffman_enforce_max_code_size(num_codes, num_used_syms,
                                        code_size_limit);

    MINIZ_CLEAR_OBJ(d->m_huff_code_sizes[table_num]);
    MINIZ_CLEAR_OBJ(d->m_huff_codes[table_num]);
    for (i = 1, j = num_used_syms; i <= code_size_limit; i++)
      for (l = num_codes[i]; l > 0; l--)
        d->m_huff_code_sizes[table_num][pSyms[--j].m_sym_index] = (miniz_uint8)(i);
  }

  next_code[1] = 0;
  for (j = 0, i = 2; i <= code_size_limit; i++)
    next_code[i] = j = ((j + num_codes[i - 1]) << 1);

  for (i = 0; i < table_len; i++) {
    miniz_uint rev_code = 0, code, code_size;
    if ((code_size = d->m_huff_code_sizes[table_num][i]) == 0)
      continue;
    code = next_code[code_size]++;
    for (l = code_size; l > 0; l--, code >>= 1)
      rev_code = (rev_code << 1) | (code & 1);
    d->m_huff_codes[table_num][i] = (miniz_uint16)rev_code;
  }
}

#define TDEFL_PUT_BITS(b, l)                                                   \
  do {                                                                         \
    miniz_uint bits = b;                                                          \
    miniz_uint len = l;                                                           \
    MINIZ_ASSERT(bits <= ((1U << len) - 1U));                                     \
    d->m_bit_buffer |= (bits << d->m_bits_in);                                 \
    d->m_bits_in += len;                                                       \
    while (d->m_bits_in >= 8) {                                                \
      if (d->m_pOutput_buf < d->m_pOutput_buf_end)                             \
        *d->m_pOutput_buf++ = (miniz_uint8)(d->m_bit_buffer);                     \
      d->m_bit_buffer >>= 8;                                                   \
      d->m_bits_in -= 8;                                                       \
    }                                                                          \
  }                                                                            \
  MINIZ_MACRO_END

#define TDEFL_RLE_PREV_CODE_SIZE()                                             \
  {                                                                            \
    if (rle_repeat_count) {                                                    \
      if (rle_repeat_count < 3) {                                              \
        d->m_huff_count[2][prev_code_size] = (miniz_uint16)(                      \
            d->m_huff_count[2][prev_code_size] + rle_repeat_count);            \
        while (rle_repeat_count--)                                             \
          packed_code_sizes[num_packed_code_sizes++] = prev_code_size;         \
      } else {                                                                 \
        d->m_huff_count[2][16] = (miniz_uint16)(d->m_huff_count[2][16] + 1);      \
        packed_code_sizes[num_packed_code_sizes++] = 16;                       \
        packed_code_sizes[num_packed_code_sizes++] =                           \
            (miniz_uint8)(rle_repeat_count - 3);                                  \
      }                                                                        \
      rle_repeat_count = 0;                                                    \
    }                                                                          \
  }

#define TDEFL_RLE_ZERO_CODE_SIZE()                                             \
  {                                                                            \
    if (rle_z_count) {                                                         \
      if (rle_z_count < 3) {                                                   \
        d->m_huff_count[2][0] =                                                \
            (miniz_uint16)(d->m_huff_count[2][0] + rle_z_count);                  \
        while (rle_z_count--)                                                  \
          packed_code_sizes[num_packed_code_sizes++] = 0;                      \
      } else if (rle_z_count <= 10) {                                          \
        d->m_huff_count[2][17] = (miniz_uint16)(d->m_huff_count[2][17] + 1);      \
        packed_code_sizes[num_packed_code_sizes++] = 17;                       \
        packed_code_sizes[num_packed_code_sizes++] =                           \
            (miniz_uint8)(rle_z_count - 3);                                       \
      } else {                                                                 \
        d->m_huff_count[2][18] = (miniz_uint16)(d->m_huff_count[2][18] + 1);      \
        packed_code_sizes[num_packed_code_sizes++] = 18;                       \
        packed_code_sizes[num_packed_code_sizes++] =                           \
            (miniz_uint8)(rle_z_count - 11);                                      \
      }                                                                        \
      rle_z_count = 0;                                                         \
    }                                                                          \
  }

static miniz_uint8 s_tdefl_packed_code_size_syms_swizzle[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static void tdefl_start_dynamic_block(tdefl_compressor *d) {
  int num_lit_codes, num_dist_codes, num_bit_lengths;
  miniz_uint i, total_code_sizes_to_pack, num_packed_code_sizes, rle_z_count,
      rle_repeat_count, packed_code_sizes_index;
  miniz_uint8
      code_sizes_to_pack[TDEFL_MAX_HUFF_SYMBOLS_0 + TDEFL_MAX_HUFF_SYMBOLS_1],
      packed_code_sizes[TDEFL_MAX_HUFF_SYMBOLS_0 + TDEFL_MAX_HUFF_SYMBOLS_1],
      prev_code_size = 0xFF;

  d->m_huff_count[0][256] = 1;

  tdefl_optimize_huffman_table(d, 0, TDEFL_MAX_HUFF_SYMBOLS_0, 15, MINIZ_FALSE);
  tdefl_optimize_huffman_table(d, 1, TDEFL_MAX_HUFF_SYMBOLS_1, 15, MINIZ_FALSE);

  for (num_lit_codes = 286; num_lit_codes > 257; num_lit_codes--)
    if (d->m_huff_code_sizes[0][num_lit_codes - 1])
      break;
  for (num_dist_codes = 30; num_dist_codes > 1; num_dist_codes--)
    if (d->m_huff_code_sizes[1][num_dist_codes - 1])
      break;

  memcpy(code_sizes_to_pack, &d->m_huff_code_sizes[0][0],
         sizeof(miniz_uint8) * num_lit_codes);
  memcpy(code_sizes_to_pack + num_lit_codes, &d->m_huff_code_sizes[1][0],
         sizeof(miniz_uint8) * num_dist_codes);
  total_code_sizes_to_pack = num_lit_codes + num_dist_codes;
  num_packed_code_sizes = 0;
  rle_z_count = 0;
  rle_repeat_count = 0;

  memset(&d->m_huff_count[2][0], 0,
         sizeof(d->m_huff_count[2][0]) * TDEFL_MAX_HUFF_SYMBOLS_2);
  for (i = 0; i < total_code_sizes_to_pack; i++) {
    miniz_uint8 code_size = code_sizes_to_pack[i];
    if (!code_size) {
      TDEFL_RLE_PREV_CODE_SIZE();
      if (++rle_z_count == 138) {
        TDEFL_RLE_ZERO_CODE_SIZE();
      }
    } else {
      TDEFL_RLE_ZERO_CODE_SIZE();
      if (code_size != prev_code_size) {
        TDEFL_RLE_PREV_CODE_SIZE();
        d->m_huff_count[2][code_size] =
            (miniz_uint16)(d->m_huff_count[2][code_size] + 1);
        packed_code_sizes[num_packed_code_sizes++] = code_size;
      } else if (++rle_repeat_count == 6) {
        TDEFL_RLE_PREV_CODE_SIZE();
      }
    }
    prev_code_size = code_size;
  }
  if (rle_repeat_count) {
    TDEFL_RLE_PREV_CODE_SIZE();
  } else {
    TDEFL_RLE_ZERO_CODE_SIZE();
  }

  tdefl_optimize_huffman_table(d, 2, TDEFL_MAX_HUFF_SYMBOLS_2, 7, MINIZ_FALSE);

  TDEFL_PUT_BITS(2, 2);

  TDEFL_PUT_BITS(num_lit_codes - 257, 5);
  TDEFL_PUT_BITS(num_dist_codes - 1, 5);

  for (num_bit_lengths = 18; num_bit_lengths >= 0; num_bit_lengths--)
    if (d->m_huff_code_sizes
            [2][s_tdefl_packed_code_size_syms_swizzle[num_bit_lengths]])
      break;
  num_bit_lengths = MINIZ_MAX(4, (num_bit_lengths + 1));
  TDEFL_PUT_BITS(num_bit_lengths - 4, 4);
  for (i = 0; (int)i < num_bit_lengths; i++)
    TDEFL_PUT_BITS(
        d->m_huff_code_sizes[2][s_tdefl_packed_code_size_syms_swizzle[i]], 3);

  for (packed_code_sizes_index = 0;
       packed_code_sizes_index < num_packed_code_sizes;) {
    miniz_uint code = packed_code_sizes[packed_code_sizes_index++];
    MINIZ_ASSERT(code < TDEFL_MAX_HUFF_SYMBOLS_2);
    TDEFL_PUT_BITS(d->m_huff_codes[2][code], d->m_huff_code_sizes[2][code]);
    if (code >= 16)
      TDEFL_PUT_BITS(packed_code_sizes[packed_code_sizes_index++],
                     "\02\03\07"[code - 16]);
  }
}

static void tdefl_start_static_block(tdefl_compressor *d) {
  miniz_uint i;
  miniz_uint8 *p = &d->m_huff_code_sizes[0][0];

  for (i = 0; i <= 143; ++i)
    *p++ = 8;
  for (; i <= 255; ++i)
    *p++ = 9;
  for (; i <= 279; ++i)
    *p++ = 7;
  for (; i <= 287; ++i)
    *p++ = 8;

  memset(d->m_huff_code_sizes[1], 5, 32);

  tdefl_optimize_huffman_table(d, 0, 288, 15, MINIZ_TRUE);
  tdefl_optimize_huffman_table(d, 1, 32, 15, MINIZ_TRUE);

  TDEFL_PUT_BITS(1, 2);
}

static const miniz_uint miniz_bitmasks[17] = {
    0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF,
    0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF};

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN &&             \
    MINIZ_HAS_64BIT_REGISTERS
static miniz_bool tdefl_compress_lz_codes(tdefl_compressor *d) {
  miniz_uint flags;
  miniz_uint8 *pLZ_codes;
  miniz_uint8 *pOutput_buf = d->m_pOutput_buf;
  miniz_uint8 *pLZ_code_buf_end = d->m_pLZ_code_buf;
  miniz_uint64 bit_buffer = d->m_bit_buffer;
  miniz_uint bits_in = d->m_bits_in;

#define TDEFL_PUT_BITS_FAST(b, l)                                              \
  {                                                                            \
    bit_buffer |= (((miniz_uint64)(b)) << bits_in);                               \
    bits_in += (l);                                                            \
  }

  flags = 1;
  for (pLZ_codes = d->m_lz_code_buf; pLZ_codes < pLZ_code_buf_end;
       flags >>= 1) {
    if (flags == 1)
      flags = *pLZ_codes++ | 0x100;

    if (flags & 1) {
      miniz_uint s0, s1, n0, n1, sym, num_extra_bits;
      miniz_uint match_len = pLZ_codes[0],
              match_dist = *(const miniz_uint16 *)(pLZ_codes + 1);
      pLZ_codes += 3;

      MINIZ_ASSERT(d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]);
      TDEFL_PUT_BITS_FAST(d->m_huff_codes[0][s_tdefl_len_sym[match_len]],
                          d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]);
      TDEFL_PUT_BITS_FAST(match_len & miniz_bitmasks[s_tdefl_len_extra[match_len]],
                          s_tdefl_len_extra[match_len]);

      // This sequence coaxes MSVC into using cmov's vs. jmp's.
      s0 = s_tdefl_small_dist_sym[match_dist & 511];
      n0 = s_tdefl_small_dist_extra[match_dist & 511];
      s1 = s_tdefl_large_dist_sym[match_dist >> 8];
      n1 = s_tdefl_large_dist_extra[match_dist >> 8];
      sym = (match_dist < 512) ? s0 : s1;
      num_extra_bits = (match_dist < 512) ? n0 : n1;

      MINIZ_ASSERT(d->m_huff_code_sizes[1][sym]);
      TDEFL_PUT_BITS_FAST(d->m_huff_codes[1][sym],
                          d->m_huff_code_sizes[1][sym]);
      TDEFL_PUT_BITS_FAST(match_dist & miniz_bitmasks[num_extra_bits],
                          num_extra_bits);
    } else {
      miniz_uint lit = *pLZ_codes++;
      MINIZ_ASSERT(d->m_huff_code_sizes[0][lit]);
      TDEFL_PUT_BITS_FAST(d->m_huff_codes[0][lit],
                          d->m_huff_code_sizes[0][lit]);

      if (((flags & 2) == 0) && (pLZ_codes < pLZ_code_buf_end)) {
        flags >>= 1;
        lit = *pLZ_codes++;
        MINIZ_ASSERT(d->m_huff_code_sizes[0][lit]);
        TDEFL_PUT_BITS_FAST(d->m_huff_codes[0][lit],
                            d->m_huff_code_sizes[0][lit]);

        if (((flags & 2) == 0) && (pLZ_codes < pLZ_code_buf_end)) {
          flags >>= 1;
          lit = *pLZ_codes++;
          MINIZ_ASSERT(d->m_huff_code_sizes[0][lit]);
          TDEFL_PUT_BITS_FAST(d->m_huff_codes[0][lit],
                              d->m_huff_code_sizes[0][lit]);
        }
      }
    }

    if (pOutput_buf >= d->m_pOutput_buf_end)
      return MINIZ_FALSE;

    *(miniz_uint64 *)pOutput_buf = bit_buffer;
    pOutput_buf += (bits_in >> 3);
    bit_buffer >>= (bits_in & ~7);
    bits_in &= 7;
  }

#undef TDEFL_PUT_BITS_FAST

  d->m_pOutput_buf = pOutput_buf;
  d->m_bits_in = 0;
  d->m_bit_buffer = 0;

  while (bits_in) {
    miniz_uint32 n = MINIZ_MIN(bits_in, 16);
    TDEFL_PUT_BITS((miniz_uint)bit_buffer & miniz_bitmasks[n], n);
    bit_buffer >>= n;
    bits_in -= n;
  }

  TDEFL_PUT_BITS(d->m_huff_codes[0][256], d->m_huff_code_sizes[0][256]);

  return (d->m_pOutput_buf < d->m_pOutput_buf_end);
}
#else
static miniz_bool tdefl_compress_lz_codes(tdefl_compressor *d) {
  miniz_uint flags;
  miniz_uint8 *pLZ_codes;

  flags = 1;
  for (pLZ_codes = d->m_lz_code_buf; pLZ_codes < d->m_pLZ_code_buf;
       flags >>= 1) {
    if (flags == 1)
      flags = *pLZ_codes++ | 0x100;
    if (flags & 1) {
      miniz_uint sym, num_extra_bits;
      miniz_uint match_len = pLZ_codes[0],
              match_dist = (pLZ_codes[1] | (pLZ_codes[2] << 8));
      pLZ_codes += 3;

      MINIZ_ASSERT(d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]);
      TDEFL_PUT_BITS(d->m_huff_codes[0][s_tdefl_len_sym[match_len]],
                     d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]);
      TDEFL_PUT_BITS(match_len & miniz_bitmasks[s_tdefl_len_extra[match_len]],
                     s_tdefl_len_extra[match_len]);

      if (match_dist < 512) {
        sym = s_tdefl_small_dist_sym[match_dist];
        num_extra_bits = s_tdefl_small_dist_extra[match_dist];
      } else {
        sym = s_tdefl_large_dist_sym[match_dist >> 8];
        num_extra_bits = s_tdefl_large_dist_extra[match_dist >> 8];
      }
      TDEFL_PUT_BITS(d->m_huff_codes[1][sym], d->m_huff_code_sizes[1][sym]);
      TDEFL_PUT_BITS(match_dist & miniz_bitmasks[num_extra_bits], num_extra_bits);
    } else {
      miniz_uint lit = *pLZ_codes++;
      MINIZ_ASSERT(d->m_huff_code_sizes[0][lit]);
      TDEFL_PUT_BITS(d->m_huff_codes[0][lit], d->m_huff_code_sizes[0][lit]);
    }
  }

  TDEFL_PUT_BITS(d->m_huff_codes[0][256], d->m_huff_code_sizes[0][256]);

  return (d->m_pOutput_buf < d->m_pOutput_buf_end);
}
#endif // MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN &&
       // MINIZ_HAS_64BIT_REGISTERS

static miniz_bool tdefl_compress_block(tdefl_compressor *d, miniz_bool static_block) {
  if (static_block)
    tdefl_start_static_block(d);
  else
    tdefl_start_dynamic_block(d);
  return tdefl_compress_lz_codes(d);
}

static int tdefl_flush_block(tdefl_compressor *d, int flush) {
  miniz_uint saved_bit_buf, saved_bits_in;
  miniz_uint8 *pSaved_output_buf;
  miniz_bool comp_block_succeeded = MINIZ_FALSE;
  int n, use_raw_block =
             ((d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS) != 0) &&
             (d->m_lookahead_pos - d->m_lz_code_buf_dict_pos) <= d->m_dict_size;
  miniz_uint8 *pOutput_buf_start =
      ((d->m_pPut_buf_func == NULL) &&
       ((*d->m_pOut_buf_size - d->m_out_buf_ofs) >= TDEFL_OUT_BUF_SIZE))
          ? ((miniz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs)
          : d->m_output_buf;

  d->m_pOutput_buf = pOutput_buf_start;
  d->m_pOutput_buf_end = d->m_pOutput_buf + TDEFL_OUT_BUF_SIZE - 16;

  MINIZ_ASSERT(!d->m_output_flush_remaining);
  d->m_output_flush_ofs = 0;
  d->m_output_flush_remaining = 0;

  *d->m_pLZ_flags = (miniz_uint8)(*d->m_pLZ_flags >> d->m_num_flags_left);
  d->m_pLZ_code_buf -= (d->m_num_flags_left == 8);

  if ((d->m_flags & TDEFL_WRITE_ZLIB_HEADER) && (!d->m_block_index)) {
    TDEFL_PUT_BITS(0x78, 8);
    TDEFL_PUT_BITS(0x01, 8);
  }

  TDEFL_PUT_BITS(flush == TDEFL_FINISH, 1);

  pSaved_output_buf = d->m_pOutput_buf;
  saved_bit_buf = d->m_bit_buffer;
  saved_bits_in = d->m_bits_in;

  if (!use_raw_block)
    comp_block_succeeded =
        tdefl_compress_block(d, (d->m_flags & TDEFL_FORCE_ALL_STATIC_BLOCKS) ||
                                    (d->m_total_lz_bytes < 48));

  // If the block gets expanded, forget the current contents of the output
  // buffer and send a raw block instead.
  if (((use_raw_block) ||
       ((d->m_total_lz_bytes) && ((d->m_pOutput_buf - pSaved_output_buf + 1U) >=
                                  d->m_total_lz_bytes))) &&
      ((d->m_lookahead_pos - d->m_lz_code_buf_dict_pos) <= d->m_dict_size)) {
    miniz_uint i;
    d->m_pOutput_buf = pSaved_output_buf;
    d->m_bit_buffer = saved_bit_buf; d->m_bits_in = saved_bits_in;
    TDEFL_PUT_BITS(0, 2);
    if (d->m_bits_in) {
      TDEFL_PUT_BITS(0, 8 - d->m_bits_in);
    }
    for (i = 2; i; --i, d->m_total_lz_bytes ^= 0xFFFF) {
      TDEFL_PUT_BITS(d->m_total_lz_bytes & 0xFFFF, 16);
    }
    for (i = 0; i < d->m_total_lz_bytes; ++i) {
      TDEFL_PUT_BITS(
          d->m_dict[(d->m_lz_code_buf_dict_pos + i) & TDEFL_LZ_DICT_SIZE_MASK],
          8);
    }
  }
  // Check for the extremely unlikely (if not impossible) case of the compressed
  // block not fitting into the output buffer when using dynamic codes.
  else if (!comp_block_succeeded) {
    d->m_pOutput_buf = pSaved_output_buf;
    d->m_bit_buffer = saved_bit_buf; d->m_bits_in = saved_bits_in;
    tdefl_compress_block(d, MINIZ_TRUE);
  }

  if (flush) {
    if (flush == TDEFL_FINISH) {
      if (d->m_bits_in) {
        TDEFL_PUT_BITS(0, 8 - d->m_bits_in);
      }
      if (d->m_flags & TDEFL_WRITE_ZLIB_HEADER) {
        miniz_uint i, a = d->m_adler32;
        for (i = 0; i < 4; i++) {
          TDEFL_PUT_BITS((a >> 24) & 0xFF, 8);
          a <<= 8;
        }
      }
    } else {
      miniz_uint i, z = 0;
      TDEFL_PUT_BITS(0, 3);
      if (d->m_bits_in) {
        TDEFL_PUT_BITS(0, 8 - d->m_bits_in);
      }
      for (i = 2; i; --i, z ^= 0xFFFF) {
        TDEFL_PUT_BITS(z & 0xFFFF, 16);
      }
    }
  }

  MINIZ_ASSERT(d->m_pOutput_buf < d->m_pOutput_buf_end);

  memset(&d->m_huff_count[0][0], 0,
         sizeof(d->m_huff_count[0][0]) * TDEFL_MAX_HUFF_SYMBOLS_0);
  memset(&d->m_huff_count[1][0], 0,
         sizeof(d->m_huff_count[1][0]) * TDEFL_MAX_HUFF_SYMBOLS_1);

  d->m_pLZ_code_buf = d->m_lz_code_buf + 1;
  d->m_pLZ_flags = d->m_lz_code_buf;
  d->m_num_flags_left = 8;
  d->m_lz_code_buf_dict_pos += d->m_total_lz_bytes;
  d->m_total_lz_bytes = 0;
  d->m_block_index++;

  if ((n = (int)(d->m_pOutput_buf - pOutput_buf_start)) != 0) {
    if (d->m_pPut_buf_func) {
      *d->m_pIn_buf_size = d->m_pSrc - (const miniz_uint8 *)d->m_pIn_buf;
      if (!(*d->m_pPut_buf_func)(d->m_output_buf, n, d->m_pPut_buf_user))
        return (d->m_prev_return_status = TDEFL_STATUS_PUT_BUF_FAILED);
    } else if (pOutput_buf_start == d->m_output_buf) {
      int bytes_to_copy = (int)MINIZ_MIN(
          (size_t)n, (size_t)(*d->m_pOut_buf_size - d->m_out_buf_ofs));
      memcpy((miniz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs, d->m_output_buf,
             bytes_to_copy);
      d->m_out_buf_ofs += bytes_to_copy;
      if ((n -= bytes_to_copy) != 0) {
        d->m_output_flush_ofs = bytes_to_copy;
        d->m_output_flush_remaining = n;
      }
    } else {
      d->m_out_buf_ofs += n;
    }
  }

  return d->m_output_flush_remaining;
}

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
#define TDEFL_READ_UNALIGNED_WORD(p) ((p)[0] | (p)[1] << 8)
static MINIZ_FORCEINLINE void
tdefl_find_match(tdefl_compressor *d, miniz_uint lookahead_pos, miniz_uint max_dist,
                 miniz_uint max_match_len, miniz_uint *pMatch_dist,
                 miniz_uint *pMatch_len) {
  miniz_uint dist, pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK,
                match_len = *pMatch_len, probe_pos = pos, next_probe_pos,
                probe_len;
  miniz_uint num_probes_left = d->m_max_probes[match_len >= 32];
  const miniz_uint16 *s = (const miniz_uint16 *)(d->m_dict + pos), *p, *q;
  miniz_uint16 c01 = TDEFL_READ_UNALIGNED_WORD(&d->m_dict[pos + match_len - 1]),
            s01 = *s;
  MINIZ_ASSERT(max_match_len <= TDEFL_MAX_MATCH_LEN);
  if (max_match_len <= match_len)
    return;
  for (;;) {
    for (;;) {
      if (--num_probes_left == 0)
        return;
#define TDEFL_PROBE                                                            \
  next_probe_pos = d->m_next[probe_pos];                                       \
  if ((!next_probe_pos) ||                                                     \
      ((dist = (miniz_uint16)(lookahead_pos - next_probe_pos)) > max_dist))       \
    return;                                                                    \
  probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK;                        \
  if (TDEFL_READ_UNALIGNED_WORD(&d->m_dict[probe_pos + match_len - 1]) == c01) \
    break;
      TDEFL_PROBE;
      TDEFL_PROBE;
      TDEFL_PROBE;
    }
    if (!dist)
      break;
    q = (const miniz_uint16 *)(d->m_dict + probe_pos);
    if (*q != s01)
      continue;
    p = s;
    probe_len = 32;
    do {
    } while ((*(++p) == *(++q)) && (*(++p) == *(++q)) && (*(++p) == *(++q)) &&
             (*(++p) == *(++q)) && (--probe_len > 0));
    if (!probe_len) {
      *pMatch_dist = dist;
      *pMatch_len = MINIZ_MIN(max_match_len, TDEFL_MAX_MATCH_LEN);
      break;
    } else if ((probe_len = ((miniz_uint)(p - s) * 2) +
                            (miniz_uint)(*(const miniz_uint8 *)p ==
                                      *(const miniz_uint8 *)q)) > match_len) {
      *pMatch_dist = dist;
      if ((*pMatch_len = match_len = MINIZ_MIN(max_match_len, probe_len)) ==
          max_match_len)
        break;
      c01 = TDEFL_READ_UNALIGNED_WORD(&d->m_dict[pos + match_len - 1]);
    }
  }
}
#else
static MINIZ_FORCEINLINE void
tdefl_find_match(tdefl_compressor *d, miniz_uint lookahead_pos, miniz_uint max_dist,
                 miniz_uint max_match_len, miniz_uint *pMatch_dist,
                 miniz_uint *pMatch_len) {
  miniz_uint dist, pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK,
                match_len = *pMatch_len, probe_pos = pos, next_probe_pos,
                probe_len;
  miniz_uint num_probes_left = d->m_max_probes[match_len >= 32];
  const miniz_uint8 *s = d->m_dict + pos, *p, *q;
  miniz_uint8 c0 = d->m_dict[pos + match_len], c1 = d->m_dict[pos + match_len - 1];
  MINIZ_ASSERT(max_match_len <= TDEFL_MAX_MATCH_LEN);
  if (max_match_len <= match_len)
    return;
  for (;;) {
    for (;;) {
      if (--num_probes_left == 0)
        return;
#define TDEFL_PROBE                                                            \
  next_probe_pos = d->m_next[probe_pos];                                       \
  if ((!next_probe_pos) ||                                                     \
      ((dist = (miniz_uint16)(lookahead_pos - next_probe_pos)) > max_dist))       \
    return;                                                                    \
  probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK;                        \
  if ((d->m_dict[probe_pos + match_len] == c0) &&                              \
      (d->m_dict[probe_pos + match_len - 1] == c1))                            \
    break;
      TDEFL_PROBE;
      TDEFL_PROBE;
      TDEFL_PROBE;
    }
    if (!dist)
      break;
    p = s;
    q = d->m_dict + probe_pos;
    for (probe_len = 0; probe_len < max_match_len; probe_len++)
      if (*p++ != *q++)
        break;
    if (probe_len > match_len) {
      *pMatch_dist = dist;
      if ((*pMatch_len = match_len = probe_len) == max_match_len)
        return;
      c0 = d->m_dict[pos + match_len];
      c1 = d->m_dict[pos + match_len - 1];
    }
  }
}
#endif // #if MINIZ_USE_UNALIGNED_LOADS_AND_STORES

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
static miniz_bool tdefl_compress_fast(tdefl_compressor *d) {
  // Faster, minimally featured LZRW1-style match+parse loop with better
  // register utilization. Intended for applications where raw throughput is
  // valued more highly than ratio.
  miniz_uint lookahead_pos = d->m_lookahead_pos,
          lookahead_size = d->m_lookahead_size, dict_size = d->m_dict_size,
          total_lz_bytes = d->m_total_lz_bytes,
          num_flags_left = d->m_num_flags_left;
  miniz_uint8 *pLZ_code_buf = d->m_pLZ_code_buf, *pLZ_flags = d->m_pLZ_flags;
  miniz_uint cur_pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK;

  while ((d->m_src_buf_left) || ((d->m_flush) && (lookahead_size))) {
    const miniz_uint TDEFL_COMP_FAST_LOOKAHEAD_SIZE = 4096;
    miniz_uint dst_pos =
        (lookahead_pos + lookahead_size) & TDEFL_LZ_DICT_SIZE_MASK;
    miniz_uint num_bytes_to_process = (miniz_uint)MINIZ_MIN(
        d->m_src_buf_left, TDEFL_COMP_FAST_LOOKAHEAD_SIZE - lookahead_size);
    d->m_src_buf_left -= num_bytes_to_process;
    lookahead_size += num_bytes_to_process;

    while (num_bytes_to_process) {
      miniz_uint32 n = MINIZ_MIN(TDEFL_LZ_DICT_SIZE - dst_pos, num_bytes_to_process);
      memcpy(d->m_dict + dst_pos, d->m_pSrc, n);
      if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
        memcpy(d->m_dict + TDEFL_LZ_DICT_SIZE + dst_pos, d->m_pSrc,
               MINIZ_MIN(n, (TDEFL_MAX_MATCH_LEN - 1) - dst_pos));
      d->m_pSrc += n;
      dst_pos = (dst_pos + n) & TDEFL_LZ_DICT_SIZE_MASK;
      num_bytes_to_process -= n;
    }

    dict_size = MINIZ_MIN(TDEFL_LZ_DICT_SIZE - lookahead_size, dict_size);
    if ((!d->m_flush) && (lookahead_size < TDEFL_COMP_FAST_LOOKAHEAD_SIZE))
      break;

    while (lookahead_size >= 4) {
      miniz_uint cur_match_dist, cur_match_len = 1;
      miniz_uint8 *pCur_dict = d->m_dict + cur_pos;
      miniz_uint first_trigram = (*(const miniz_uint32 *)pCur_dict) & 0xFFFFFF;
      miniz_uint hash =
          (first_trigram ^ (first_trigram >> (24 - (TDEFL_LZ_HASH_BITS - 8)))) &
          TDEFL_LEVEL1_HASH_SIZE_MASK;
      miniz_uint probe_pos = d->m_hash[hash];
      d->m_hash[hash] = (miniz_uint16)lookahead_pos;

      if (((cur_match_dist = (miniz_uint16)(lookahead_pos - probe_pos)) <=
           dict_size) &&
          ((miniz_uint32)(
               *(d->m_dict + (probe_pos & TDEFL_LZ_DICT_SIZE_MASK)) |
               (*(d->m_dict + ((probe_pos & TDEFL_LZ_DICT_SIZE_MASK) + 1))
                << 8) |
               (*(d->m_dict + ((probe_pos & TDEFL_LZ_DICT_SIZE_MASK) + 2))
                << 16)) == first_trigram)) {
        const miniz_uint16 *p = (const miniz_uint16 *)pCur_dict;
        const miniz_uint16 *q =
            (const miniz_uint16 *)(d->m_dict +
                                (probe_pos & TDEFL_LZ_DICT_SIZE_MASK));
        miniz_uint32 probe_len = 32;
        do {
        } while ((*(++p) == *(++q)) && (*(++p) == *(++q)) &&
                 (*(++p) == *(++q)) && (*(++p) == *(++q)) && (--probe_len > 0));
        cur_match_len = ((miniz_uint)(p - (const miniz_uint16 *)pCur_dict) * 2) +
                        (miniz_uint)(*(const miniz_uint8 *)p == *(const miniz_uint8 *)q);
        if (!probe_len)
          cur_match_len = cur_match_dist ? TDEFL_MAX_MATCH_LEN : 0;

        if ((cur_match_len < TDEFL_MIN_MATCH_LEN) ||
            ((cur_match_len == TDEFL_MIN_MATCH_LEN) &&
             (cur_match_dist >= 8U * 1024U))) {
          cur_match_len = 1;
          *pLZ_code_buf++ = (miniz_uint8)first_trigram;
          *pLZ_flags = (miniz_uint8)(*pLZ_flags >> 1);
          d->m_huff_count[0][(miniz_uint8)first_trigram]++;
        } else {
          miniz_uint32 s0, s1;
          cur_match_len = MINIZ_MIN(cur_match_len, lookahead_size);

          MINIZ_ASSERT((cur_match_len >= TDEFL_MIN_MATCH_LEN) &&
                    (cur_match_dist >= 1) &&
                    (cur_match_dist <= TDEFL_LZ_DICT_SIZE));

          cur_match_dist--;

          pLZ_code_buf[0] = (miniz_uint8)(cur_match_len - TDEFL_MIN_MATCH_LEN);
          *(miniz_uint16 *)(&pLZ_code_buf[1]) = (miniz_uint16)cur_match_dist;
          pLZ_code_buf += 3;
          *pLZ_flags = (miniz_uint8)((*pLZ_flags >> 1) | 0x80);

          s0 = s_tdefl_small_dist_sym[cur_match_dist & 511];
          s1 = s_tdefl_large_dist_sym[cur_match_dist >> 8];
          d->m_huff_count[1][(cur_match_dist < 512) ? s0 : s1]++;

          d->m_huff_count[0][s_tdefl_len_sym[cur_match_len -
                                             TDEFL_MIN_MATCH_LEN]]++;
        }
      } else {
        *pLZ_code_buf++ = (miniz_uint8)first_trigram;
        *pLZ_flags = (miniz_uint8)(*pLZ_flags >> 1);
        d->m_huff_count[0][(miniz_uint8)first_trigram]++;
      }

      if (--num_flags_left == 0) {
        num_flags_left = 8;
        pLZ_flags = pLZ_code_buf++;
      }

      total_lz_bytes += cur_match_len;
      lookahead_pos += cur_match_len;
      dict_size = MINIZ_MIN(dict_size + cur_match_len, TDEFL_LZ_DICT_SIZE);
      cur_pos = (cur_pos + cur_match_len) & TDEFL_LZ_DICT_SIZE_MASK;
      MINIZ_ASSERT(lookahead_size >= cur_match_len);
      lookahead_size -= cur_match_len;

      if (pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8]) {
        int n;
        d->m_lookahead_pos = lookahead_pos;
        d->m_lookahead_size = lookahead_size;
        d->m_dict_size = dict_size;
        d->m_total_lz_bytes = total_lz_bytes;
        d->m_pLZ_code_buf = pLZ_code_buf;
        d->m_pLZ_flags = pLZ_flags;
        d->m_num_flags_left = num_flags_left;
        if ((n = tdefl_flush_block(d, 0)) != 0)
          return (n < 0) ? MINIZ_FALSE : MINIZ_TRUE;
        total_lz_bytes = d->m_total_lz_bytes;
        pLZ_code_buf = d->m_pLZ_code_buf;
        pLZ_flags = d->m_pLZ_flags;
        num_flags_left = d->m_num_flags_left;
      }
    }

    while (lookahead_size) {
      miniz_uint8 lit = d->m_dict[cur_pos];

      total_lz_bytes++;
      *pLZ_code_buf++ = lit;
      *pLZ_flags = (miniz_uint8)(*pLZ_flags >> 1);
      if (--num_flags_left == 0) {
        num_flags_left = 8;
        pLZ_flags = pLZ_code_buf++;
      }

      d->m_huff_count[0][lit]++;

      lookahead_pos++;
      dict_size = MINIZ_MIN(dict_size + 1, TDEFL_LZ_DICT_SIZE);
      cur_pos = (cur_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK;
      lookahead_size--;

      if (pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8]) {
        int n;
        d->m_lookahead_pos = lookahead_pos;
        d->m_lookahead_size = lookahead_size;
        d->m_dict_size = dict_size;
        d->m_total_lz_bytes = total_lz_bytes;
        d->m_pLZ_code_buf = pLZ_code_buf;
        d->m_pLZ_flags = pLZ_flags;
        d->m_num_flags_left = num_flags_left;
        if ((n = tdefl_flush_block(d, 0)) != 0)
          return (n < 0) ? MINIZ_FALSE : MINIZ_TRUE;
        total_lz_bytes = d->m_total_lz_bytes;
        pLZ_code_buf = d->m_pLZ_code_buf;
        pLZ_flags = d->m_pLZ_flags;
        num_flags_left = d->m_num_flags_left;
      }
    }
  }

  d->m_lookahead_pos = lookahead_pos;
  d->m_lookahead_size = lookahead_size;
  d->m_dict_size = dict_size;
  d->m_total_lz_bytes = total_lz_bytes;
  d->m_pLZ_code_buf = pLZ_code_buf;
  d->m_pLZ_flags = pLZ_flags;
  d->m_num_flags_left = num_flags_left;
  return MINIZ_TRUE;
}
#endif // MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN

static MINIZ_FORCEINLINE void tdefl_record_literal(tdefl_compressor *d,
                                                miniz_uint8 lit) {
  d->m_total_lz_bytes++;
  *d->m_pLZ_code_buf++ = lit;
  *d->m_pLZ_flags = (miniz_uint8)(*d->m_pLZ_flags >> 1);
  if (--d->m_num_flags_left == 0) {
    d->m_num_flags_left = 8;
    d->m_pLZ_flags = d->m_pLZ_code_buf++;
  }
  d->m_huff_count[0][lit]++;
}

static MINIZ_FORCEINLINE void
tdefl_record_match(tdefl_compressor *d, miniz_uint match_len, miniz_uint match_dist) {
  miniz_uint32 s0, s1;

  MINIZ_ASSERT((match_len >= TDEFL_MIN_MATCH_LEN) && (match_dist >= 1) &&
            (match_dist <= TDEFL_LZ_DICT_SIZE));

  d->m_total_lz_bytes += match_len;

  d->m_pLZ_code_buf[0] = (miniz_uint8)(match_len - TDEFL_MIN_MATCH_LEN);

  match_dist -= 1;
  d->m_pLZ_code_buf[1] = (miniz_uint8)(match_dist & 0xFF);
  d->m_pLZ_code_buf[2] = (miniz_uint8)(match_dist >> 8);
  d->m_pLZ_code_buf += 3;

  *d->m_pLZ_flags = (miniz_uint8)((*d->m_pLZ_flags >> 1) | 0x80);
  if (--d->m_num_flags_left == 0) {
    d->m_num_flags_left = 8;
    d->m_pLZ_flags = d->m_pLZ_code_buf++;
  }

  s0 = s_tdefl_small_dist_sym[match_dist & 511];
  s1 = s_tdefl_large_dist_sym[(match_dist >> 8) & 127];
  d->m_huff_count[1][(match_dist < 512) ? s0 : s1]++;

  if (match_len >= TDEFL_MIN_MATCH_LEN)
    d->m_huff_count[0][s_tdefl_len_sym[match_len - TDEFL_MIN_MATCH_LEN]]++;
}

static miniz_bool tdefl_compress_normal(tdefl_compressor *d) {
  const miniz_uint8 *pSrc = d->m_pSrc;
  size_t src_buf_left = d->m_src_buf_left;
  tdefl_flush flush = d->m_flush;

  while ((src_buf_left) || ((flush) && (d->m_lookahead_size))) {
    miniz_uint len_to_move, cur_match_dist, cur_match_len, cur_pos;
    // Update dictionary and hash chains. Keeps the lookahead size equal to
    // TDEFL_MAX_MATCH_LEN.
    if ((d->m_lookahead_size + d->m_dict_size) >= (TDEFL_MIN_MATCH_LEN - 1)) {
      miniz_uint dst_pos = (d->m_lookahead_pos + d->m_lookahead_size) &
                        TDEFL_LZ_DICT_SIZE_MASK,
              ins_pos = d->m_lookahead_pos + d->m_lookahead_size - 2;
      miniz_uint hash = (d->m_dict[ins_pos & TDEFL_LZ_DICT_SIZE_MASK]
                      << TDEFL_LZ_HASH_SHIFT) ^
                     d->m_dict[(ins_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK];
      miniz_uint num_bytes_to_process = (miniz_uint)MINIZ_MIN(
          src_buf_left, TDEFL_MAX_MATCH_LEN - d->m_lookahead_size);
      const miniz_uint8 *pSrc_end = pSrc + num_bytes_to_process;
      src_buf_left -= num_bytes_to_process;
      d->m_lookahead_size += num_bytes_to_process;
      while (pSrc != pSrc_end) {
        miniz_uint8 c = *pSrc++;
        d->m_dict[dst_pos] = c;
        if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
          d->m_dict[TDEFL_LZ_DICT_SIZE + dst_pos] = c;
        hash = ((hash << TDEFL_LZ_HASH_SHIFT) ^ c) & (TDEFL_LZ_HASH_SIZE - 1);
        d->m_next[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] = d->m_hash[hash];
        d->m_hash[hash] = (miniz_uint16)(ins_pos);
        dst_pos = (dst_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK;
        ins_pos++;
      }
    } else {
      while ((src_buf_left) && (d->m_lookahead_size < TDEFL_MAX_MATCH_LEN)) {
        miniz_uint8 c = *pSrc++;
        miniz_uint dst_pos = (d->m_lookahead_pos + d->m_lookahead_size) &
                          TDEFL_LZ_DICT_SIZE_MASK;
        src_buf_left--;
        d->m_dict[dst_pos] = c;
        if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
          d->m_dict[TDEFL_LZ_DICT_SIZE + dst_pos] = c;
        if ((++d->m_lookahead_size + d->m_dict_size) >= TDEFL_MIN_MATCH_LEN) {
          miniz_uint ins_pos = d->m_lookahead_pos + (d->m_lookahead_size - 1) - 2;
          miniz_uint hash = ((d->m_dict[ins_pos & TDEFL_LZ_DICT_SIZE_MASK]
                           << (TDEFL_LZ_HASH_SHIFT * 2)) ^
                          (d->m_dict[(ins_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK]
                           << TDEFL_LZ_HASH_SHIFT) ^
                          c) &
                         (TDEFL_LZ_HASH_SIZE - 1);
          d->m_next[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] = d->m_hash[hash];
          d->m_hash[hash] = (miniz_uint16)(ins_pos);
        }
      }
    }
    d->m_dict_size =
        MINIZ_MIN(TDEFL_LZ_DICT_SIZE - d->m_lookahead_size, d->m_dict_size);
    if ((!flush) && (d->m_lookahead_size < TDEFL_MAX_MATCH_LEN))
      break;

    // Simple lazy/greedy parsing state machine.
    len_to_move = 1;
    cur_match_dist = 0;
    cur_match_len =
        d->m_saved_match_len ? d->m_saved_match_len : (TDEFL_MIN_MATCH_LEN - 1);
    cur_pos = d->m_lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK;
    if (d->m_flags & (TDEFL_RLE_MATCHES | TDEFL_FORCE_ALL_RAW_BLOCKS)) {
      if ((d->m_dict_size) && (!(d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS))) {
        miniz_uint8 c = d->m_dict[(cur_pos - 1) & TDEFL_LZ_DICT_SIZE_MASK];
        cur_match_len = 0;
        while (cur_match_len < d->m_lookahead_size) {
          if (d->m_dict[cur_pos + cur_match_len] != c)
            break;
          cur_match_len++;
        }
        if (cur_match_len < TDEFL_MIN_MATCH_LEN)
          cur_match_len = 0;
        else
          cur_match_dist = 1;
      }
    } else {
      tdefl_find_match(d, d->m_lookahead_pos, d->m_dict_size,
                       d->m_lookahead_size, &cur_match_dist, &cur_match_len);
    }
    if (((cur_match_len == TDEFL_MIN_MATCH_LEN) &&
         (cur_match_dist >= 8U * 1024U)) ||
        (cur_pos == cur_match_dist) ||
        ((d->m_flags & TDEFL_FILTER_MATCHES) && (cur_match_len <= 5))) {
      cur_match_dist = cur_match_len = 0;
    }
    if (d->m_saved_match_len) {
      if (cur_match_len > d->m_saved_match_len) {
        tdefl_record_literal(d, (miniz_uint8)d->m_saved_lit);
        if (cur_match_len >= 128) {
          tdefl_record_match(d, cur_match_len, cur_match_dist);
          d->m_saved_match_len = 0;
          len_to_move = cur_match_len;
        } else {
          d->m_saved_lit = d->m_dict[cur_pos];
          d->m_saved_match_dist = cur_match_dist;
          d->m_saved_match_len = cur_match_len;
        }
      } else {
        tdefl_record_match(d, d->m_saved_match_len, d->m_saved_match_dist);
        len_to_move = d->m_saved_match_len - 1;
        d->m_saved_match_len = 0;
      }
    } else if (!cur_match_dist)
      tdefl_record_literal(d,
                           d->m_dict[MINIZ_MIN(cur_pos, sizeof(d->m_dict) - 1)]);
    else if ((d->m_greedy_parsing) || (d->m_flags & TDEFL_RLE_MATCHES) ||
             (cur_match_len >= 128)) {
      tdefl_record_match(d, cur_match_len, cur_match_dist);
      len_to_move = cur_match_len;
    } else {
      d->m_saved_lit = d->m_dict[MINIZ_MIN(cur_pos, sizeof(d->m_dict) - 1)];
      d->m_saved_match_dist = cur_match_dist;
      d->m_saved_match_len = cur_match_len;
    }
    // Move the lookahead forward by len_to_move bytes.
    d->m_lookahead_pos += len_to_move;
    MINIZ_ASSERT(d->m_lookahead_size >= len_to_move);
    d->m_lookahead_size -= len_to_move;
    d->m_dict_size = MINIZ_MIN(d->m_dict_size + len_to_move, TDEFL_LZ_DICT_SIZE);
    // Check if it's time to flush the current LZ codes to the internal output
    // buffer.
    if ((d->m_pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8]) ||
        ((d->m_total_lz_bytes > 31 * 1024) &&
         (((((miniz_uint)(d->m_pLZ_code_buf - d->m_lz_code_buf) * 115) >> 7) >=
           d->m_total_lz_bytes) ||
          (d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS)))) {
      int n;
      d->m_pSrc = pSrc;
      d->m_src_buf_left = src_buf_left;
      if ((n = tdefl_flush_block(d, 0)) != 0)
        return (n < 0) ? MINIZ_FALSE : MINIZ_TRUE;
    }
  }

  d->m_pSrc = pSrc;
  d->m_src_buf_left = src_buf_left;
  return MINIZ_TRUE;
}

static tdefl_status tdefl_flush_output_buffer(tdefl_compressor *d) {
  if (d->m_pIn_buf_size) {
    *d->m_pIn_buf_size = d->m_pSrc - (const miniz_uint8 *)d->m_pIn_buf;
  }

  if (d->m_pOut_buf_size) {
    size_t n = MINIZ_MIN(*d->m_pOut_buf_size - d->m_out_buf_ofs,
                      d->m_output_flush_remaining);
    memcpy((miniz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs,
           d->m_output_buf + d->m_output_flush_ofs, n);
    d->m_output_flush_ofs += (miniz_uint)n;
    d->m_output_flush_remaining -= (miniz_uint)n;
    d->m_out_buf_ofs += n;

    *d->m_pOut_buf_size = d->m_out_buf_ofs;
  }

  return (d->m_finished && !d->m_output_flush_remaining) ? TDEFL_STATUS_DONE
                                                         : TDEFL_STATUS_OKAY;
}

tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf,
                            size_t *pIn_buf_size, void *pOut_buf,
                            size_t *pOut_buf_size, tdefl_flush flush) {
  if (!d) {
    if (pIn_buf_size)
      *pIn_buf_size = 0;
    if (pOut_buf_size)
      *pOut_buf_size = 0;
    return TDEFL_STATUS_BAD_PARAM;
  }

  d->m_pIn_buf = pIn_buf;
  d->m_pIn_buf_size = pIn_buf_size;
  d->m_pOut_buf = pOut_buf;
  d->m_pOut_buf_size = pOut_buf_size;
  d->m_pSrc = (const miniz_uint8 *)(pIn_buf);
  d->m_src_buf_left = pIn_buf_size ? *pIn_buf_size : 0;
  d->m_out_buf_ofs = 0;
  d->m_flush = flush;

  if (((d->m_pPut_buf_func != NULL) ==
       ((pOut_buf != NULL) || (pOut_buf_size != NULL))) ||
      (d->m_prev_return_status != TDEFL_STATUS_OKAY) ||
      (d->m_wants_to_finish && (flush != TDEFL_FINISH)) ||
      (pIn_buf_size && *pIn_buf_size && !pIn_buf) ||
      (pOut_buf_size && *pOut_buf_size && !pOut_buf)) {
    if (pIn_buf_size)
      *pIn_buf_size = 0;
    if (pOut_buf_size)
      *pOut_buf_size = 0;
    return (d->m_prev_return_status = TDEFL_STATUS_BAD_PARAM);
  }
  d->m_wants_to_finish |= (flush == TDEFL_FINISH);

  if ((d->m_output_flush_remaining) || (d->m_finished))
    return (d->m_prev_return_status = tdefl_flush_output_buffer(d));

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
  if (((d->m_flags & TDEFL_MAX_PROBES_MASK) == 1) &&
      ((d->m_flags & TDEFL_GREEDY_PARSING_FLAG) != 0) &&
      ((d->m_flags & (TDEFL_FILTER_MATCHES | TDEFL_FORCE_ALL_RAW_BLOCKS |
                      TDEFL_RLE_MATCHES)) == 0)) {
    if (!tdefl_compress_fast(d))
      return d->m_prev_return_status;
  } else
#endif // #if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
  {
    if (!tdefl_compress_normal(d))
      return d->m_prev_return_status;
  }

  if ((d->m_flags & (TDEFL_WRITE_ZLIB_HEADER | TDEFL_COMPUTE_ADLER32)) &&
      (pIn_buf))
    d->m_adler32 =
        (miniz_uint32)miniz_adler32(d->m_adler32, (const miniz_uint8 *)pIn_buf,
                              d->m_pSrc - (const miniz_uint8 *)pIn_buf);

  if ((flush) && (!d->m_lookahead_size) && (!d->m_src_buf_left) &&
      (!d->m_output_flush_remaining)) {
    if (tdefl_flush_block(d, flush) < 0)
      return d->m_prev_return_status;
    d->m_finished = (flush == TDEFL_FINISH);
    if (flush == TDEFL_FULL_FLUSH) {
      MINIZ_CLEAR_OBJ(d->m_hash);
      MINIZ_CLEAR_OBJ(d->m_next);
      d->m_dict_size = 0;
    }
  }

  return (d->m_prev_return_status = tdefl_flush_output_buffer(d));
}

tdefl_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf,
                                   size_t in_buf_size, tdefl_flush flush) {
  MINIZ_ASSERT(d->m_pPut_buf_func);
  return tdefl_compress(d, pIn_buf, &in_buf_size, NULL, NULL, flush);
}

tdefl_status tdefl_init(tdefl_compressor *d,
                        tdefl_put_buf_func_ptr pPut_buf_func,
                        void *pPut_buf_user, int flags) {
  d->m_pPut_buf_func = pPut_buf_func;
  d->m_pPut_buf_user = pPut_buf_user;
  d->m_flags = (miniz_uint)(flags);
  d->m_max_probes[0] = 1 + ((flags & 0xFFF) + 2) / 3;
  d->m_greedy_parsing = (flags & TDEFL_GREEDY_PARSING_FLAG) != 0;
  d->m_max_probes[1] = 1 + (((flags & 0xFFF) >> 2) + 2) / 3;
  if (!(flags & TDEFL_NONDETERMINISTIC_PARSING_FLAG))
    MINIZ_CLEAR_OBJ(d->m_hash);
  d->m_lookahead_pos = d->m_lookahead_size = d->m_dict_size =
      d->m_total_lz_bytes = d->m_lz_code_buf_dict_pos = d->m_bits_in = 0;
  d->m_output_flush_ofs = d->m_output_flush_remaining = d->m_finished =
      d->m_block_index = d->m_bit_buffer = d->m_wants_to_finish = 0;
  d->m_pLZ_code_buf = d->m_lz_code_buf + 1;
  d->m_pLZ_flags = d->m_lz_code_buf;
  d->m_num_flags_left = 8;
  d->m_pOutput_buf = d->m_output_buf;
  d->m_pOutput_buf_end = d->m_output_buf;
  d->m_prev_return_status = TDEFL_STATUS_OKAY;
  d->m_saved_match_dist = d->m_saved_match_len = d->m_saved_lit = 0;
  d->m_adler32 = 1;
  d->m_pIn_buf = NULL;
  d->m_pOut_buf = NULL;
  d->m_pIn_buf_size = NULL;
  d->m_pOut_buf_size = NULL;
  d->m_flush = TDEFL_NO_FLUSH;
  d->m_pSrc = NULL;
  d->m_src_buf_left = 0;
  d->m_out_buf_ofs = 0;
  memset(&d->m_huff_count[0][0], 0,
         sizeof(d->m_huff_count[0][0]) * TDEFL_MAX_HUFF_SYMBOLS_0);
  memset(&d->m_huff_count[1][0], 0,
         sizeof(d->m_huff_count[1][0]) * TDEFL_MAX_HUFF_SYMBOLS_1);
  return TDEFL_STATUS_OKAY;
}

tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d) {
  return d->m_prev_return_status;
}

miniz_uint32 tdefl_get_adler32(tdefl_compressor *d) { return d->m_adler32; }

miniz_bool tdefl_compress_mem_to_output(const void *pBuf, size_t buf_len,
                                     tdefl_put_buf_func_ptr pPut_buf_func,
                                     void *pPut_buf_user, int flags) {
  tdefl_compressor *pComp;
  miniz_bool succeeded;
  if (((buf_len) && (!pBuf)) || (!pPut_buf_func))
    return MINIZ_FALSE;
  pComp = (tdefl_compressor *)MINIZ_MALLOC(sizeof(tdefl_compressor));
  if (!pComp)
    return MINIZ_FALSE;
  succeeded = (tdefl_init(pComp, pPut_buf_func, pPut_buf_user, flags) ==
               TDEFL_STATUS_OKAY);
  succeeded =
      succeeded && (tdefl_compress_buffer(pComp, pBuf, buf_len, TDEFL_FINISH) ==
                    TDEFL_STATUS_DONE);
  MINIZ_FREE(pComp);
  return succeeded;
}

typedef struct {
  size_t m_size, m_capacity;
  miniz_uint8 *m_pBuf;
  miniz_bool m_expandable;
} tdefl_output_buffer;

static miniz_bool tdefl_output_buffer_putter(const void *pBuf, int len,
                                          void *pUser) {
  tdefl_output_buffer *p = (tdefl_output_buffer *)pUser;
  size_t new_size = p->m_size + len;
  if (new_size > p->m_capacity) {
    size_t new_capacity = p->m_capacity;
    miniz_uint8 *pNew_buf;
    if (!p->m_expandable)
      return MINIZ_FALSE;
    do {
      new_capacity = MINIZ_MAX(128U, new_capacity << 1U);
    } while (new_size > new_capacity);
    pNew_buf = (miniz_uint8 *)MINIZ_REALLOC(p->m_pBuf, new_capacity);
    if (!pNew_buf)
      return MINIZ_FALSE;
    p->m_pBuf = pNew_buf;
    p->m_capacity = new_capacity;
  }
  memcpy((miniz_uint8 *)p->m_pBuf + p->m_size, pBuf, len);
  p->m_size = new_size;
  return MINIZ_TRUE;
}

void *tdefl_compress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len,
                                 size_t *pOut_len, int flags) {
  tdefl_output_buffer out_buf;
  MINIZ_CLEAR_OBJ(out_buf);
  if (!pOut_len)
    return MINIZ_FALSE;
  else
    *pOut_len = 0;
  out_buf.m_expandable = MINIZ_TRUE;
  if (!tdefl_compress_mem_to_output(
          pSrc_buf, src_buf_len, tdefl_output_buffer_putter, &out_buf, flags))
    return NULL;
  *pOut_len = out_buf.m_size;
  return out_buf.m_pBuf;
}

size_t tdefl_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len,
                                 const void *pSrc_buf, size_t src_buf_len,
                                 int flags) {
  tdefl_output_buffer out_buf;
  MINIZ_CLEAR_OBJ(out_buf);
  if (!pOut_buf)
    return 0;
  out_buf.m_pBuf = (miniz_uint8 *)pOut_buf;
  out_buf.m_capacity = out_buf_len;
  if (!tdefl_compress_mem_to_output(
          pSrc_buf, src_buf_len, tdefl_output_buffer_putter, &out_buf, flags))
    return 0;
  return out_buf.m_size;
}

#ifndef MINIZ_NO_ZLIB_APIS
static const miniz_uint s_tdefl_num_probes[11] = {0,   1,   6,   32,  16,  32,
                                               128, 256, 512, 768, 1500};

// level may actually range from [0,10] (10 is a "hidden" max level, where we
// want a bit more compression and it's fine if throughput to fall off a cliff
// on some files).
miniz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits,
                                                int strategy) {
  miniz_uint comp_flags =
      s_tdefl_num_probes[(level >= 0) ? MINIZ_MIN(10, level) : MINIZ_DEFAULT_LEVEL] |
      ((level <= 3) ? TDEFL_GREEDY_PARSING_FLAG : 0);
  if (window_bits > 0)
    comp_flags |= TDEFL_WRITE_ZLIB_HEADER;

  if (!level)
    comp_flags |= TDEFL_FORCE_ALL_RAW_BLOCKS;
  else if (strategy == MINIZ_FILTERED)
    comp_flags |= TDEFL_FILTER_MATCHES;
  else if (strategy == MINIZ_HUFFMAN_ONLY)
    comp_flags &= ~TDEFL_MAX_PROBES_MASK;
  else if (strategy == MINIZ_FIXED)
    comp_flags |= TDEFL_FORCE_ALL_STATIC_BLOCKS;
  else if (strategy == MINIZ_RLE)
    comp_flags |= TDEFL_RLE_MATCHES;

  return comp_flags;
}
#endif // MINIZ_NO_ZLIB_APIS

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4204) // nonstandard extension used : non-constant
                                // aggregate initializer (also supported by GNU
                                // C and C99, so no big deal)
#endif

// Simple PNG writer function by Alex Evans, 2011. Released into the public
// domain: https://gist.github.com/908299, more context at
// http://altdevblogaday.org/2011/04/06/a-smaller-jpg-encoder/.
// This is actually a modification of Alex's original code so PNG files
// generated by this function pass pngcheck.
void *tdefl_write_image_to_png_file_in_memory_ex(const void *pImage, int w,
                                                 int h, int num_chans,
                                                 size_t *pLen_out,
                                                 miniz_uint level, miniz_bool flip) {
  // Using a local copy of this array here in case MINIZ_NO_ZLIB_APIS was
  // defined.
  static const miniz_uint s_tdefl_png_num_probes[11] = {
      0, 1, 6, 32, 16, 32, 128, 256, 512, 768, 1500};
  tdefl_compressor *pComp =
      (tdefl_compressor *)MINIZ_MALLOC(sizeof(tdefl_compressor));
  tdefl_output_buffer out_buf;
  int i, bpl = w * num_chans, y, z;
  miniz_uint32 c;
  *pLen_out = 0;
  if (!pComp)
    return NULL;
  MINIZ_CLEAR_OBJ(out_buf);
  out_buf.m_expandable = MINIZ_TRUE;
  out_buf.m_capacity = 57 + MINIZ_MAX(64, (1 + bpl) * h);
  if (NULL == (out_buf.m_pBuf = (miniz_uint8 *)MINIZ_MALLOC(out_buf.m_capacity))) {
    MINIZ_FREE(pComp);
    return NULL;
  }
  // write dummy header
  for (z = 41; z; --z)
    tdefl_output_buffer_putter(&z, 1, &out_buf);
  // compress image data
  tdefl_init(pComp, tdefl_output_buffer_putter, &out_buf,
             s_tdefl_png_num_probes[MINIZ_MIN(10, level)] |
                 TDEFL_WRITE_ZLIB_HEADER);
  for (y = 0; y < h; ++y) {
    tdefl_compress_buffer(pComp, &z, 1, TDEFL_NO_FLUSH);
    tdefl_compress_buffer(pComp,
                          (miniz_uint8 *)pImage + (flip ? (h - 1 - y) : y) * bpl,
                          bpl, TDEFL_NO_FLUSH);
  }
  if (tdefl_compress_buffer(pComp, NULL, 0, TDEFL_FINISH) !=
      TDEFL_STATUS_DONE) {
    MINIZ_FREE(pComp);
    MINIZ_FREE(out_buf.m_pBuf);
    return NULL;
  }
  // write real header
  *pLen_out = out_buf.m_size - 41;
  {
    static const miniz_uint8 chans[] = {0x00, 0x00, 0x04, 0x02, 0x06};
    miniz_uint8 pnghdr[41] = {0x89,
                           0x50,
                           0x4e,
                           0x47,
                           0x0d,
                           0x0a,
                           0x1a,
                           0x0a,
                           0x00,
                           0x00,
                           0x00,
                           0x0d,
                           0x49,
                           0x48,
                           0x44,
                           0x52,
                           0,
                           0,
                           (miniz_uint8)(w >> 8),
                           (miniz_uint8)w,
                           0,
                           0,
                           (miniz_uint8)(h >> 8),
                           (miniz_uint8)h,
                           8,
                           chans[num_chans],
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           (miniz_uint8)(*pLen_out >> 24),
                           (miniz_uint8)(*pLen_out >> 16),
                           (miniz_uint8)(*pLen_out >> 8),
                           (miniz_uint8)*pLen_out,
                           0x49,
                           0x44,
                           0x41,
                           0x54};
    c = (miniz_uint32)miniz_crc32(MINIZ_CRC32_INIT, pnghdr + 12, 17);
    for (i = 0; i < 4; ++i, c <<= 8)
      ((miniz_uint8 *)(pnghdr + 29))[i] = (miniz_uint8)(c >> 24);
    memcpy(out_buf.m_pBuf, pnghdr, 41);
  }
  // write footer (IDAT CRC-32, followed by IEND chunk)
  if (!tdefl_output_buffer_putter(
          "\0\0\0\0\0\0\0\0\x49\x45\x4e\x44\xae\x42\x60\x82", 16, &out_buf)) {
    *pLen_out = 0;
    MINIZ_FREE(pComp);
    MINIZ_FREE(out_buf.m_pBuf);
    return NULL;
  }
  c = (miniz_uint32)miniz_crc32(MINIZ_CRC32_INIT, out_buf.m_pBuf + 41 - 4,
                          *pLen_out + 4);
  for (i = 0; i < 4; ++i, c <<= 8)
    (out_buf.m_pBuf + out_buf.m_size - 16)[i] = (miniz_uint8)(c >> 24);
  // compute final size of file, grab compressed data buffer and return
  *pLen_out += 57;
  MINIZ_FREE(pComp);
  return out_buf.m_pBuf;
}
void *tdefl_write_image_to_png_file_in_memory(const void *pImage, int w, int h,
                                              int num_chans, size_t *pLen_out) {
  // Level 6 corresponds to TDEFL_DEFAULT_MAX_PROBES or MINIZ_DEFAULT_LEVEL (but we
  // can't depend on MINIZ_DEFAULT_LEVEL being available in case the zlib API's
  // where #defined out)
  return tdefl_write_image_to_png_file_in_memory_ex(pImage, w, h, num_chans,
                                                    pLen_out, 6, MINIZ_FALSE);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// ------------------- .ZIP archive reading

#ifndef MINIZ_NO_ARCHIVE_APIS

#ifdef MINIZ_NO_STDIO
#define MINIZ_FILE void *
#else
#include <stdio.h>
#include <sys/stat.h>

// CONFFX_BEGIN - Custom File IO
#ifdef MINIZ_FORGE_IO
#define MINIZ_FILE FileStream
#ifdef _MSC_VER
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif

#define MINIZ_FILE_STAT_STRUCT _stat
#define MINIZ_FILE_STAT _stat
#else
#define MINIZ_FILE_STAT_STRUCT stat
#define MINIZ_FILE_STAT stat
#endif

static bool MINIZ_FOPEN(const ResourceDirectory resourceDirectory, const char *fileName, const char* filePassword, const char *pMode, MINIZ_FILE* pOut)
{
	return fsOpenStreamFromPath(resourceDirectory, fileName, fsFileModeFromString(pMode), filePassword, pOut);
}
static bool MINIZ_FREOPEN(const ResourceDirectory resourceDirectory, const char* fileName, const char *pMode, MINIZ_FILE* pStream)
{
	return false;
}
static int MINIZ_FCLOSE(MINIZ_FILE* file)
{
	int ret = fsCloseStream(file) ? 0 : EOF;
	return ret;
}
static size_t MINIZ_FREAD(void* _Buffer, size_t _ElementSize, size_t _ElementCount, MINIZ_FILE* _Stream)
{
	return fsReadFromStream(_Stream, _Buffer, _ElementSize * _ElementCount);
}
static size_t MINIZ_FWRITE(void const* _Buffer, size_t _ElementSize, size_t _ElementCount, MINIZ_FILE* _Stream)
{
	return fsWriteToStream(_Stream, _Buffer, _ElementSize * _ElementCount);
}
static int64_t MINIZ_FTELL64(MINIZ_FILE* _Stream)
{
	return (int64_t)fsGetStreamSeekPosition(_Stream);
}
static int MINIZ_FSEEK64(MINIZ_FILE* _Stream, int64_t _Offset, int _Origin)
{
	SeekBaseOffset origin = SBO_START_OF_FILE;
	switch (_Origin)
	{
	case SEEK_SET: origin = SBO_START_OF_FILE; break;
	case SEEK_CUR: origin = SBO_CURRENT_POSITION; break;
	case SEEK_END: origin = SBO_END_OF_FILE; break;
	}
	return fsSeekStream(_Stream, origin, _Offset) ? 0 : EOF;
}
static int MINIZ_FFLUSH(MINIZ_FILE* _Stream)
{
	fsFlushStream(_Stream);
	return 0;
}
static int MINIZ_DELETE_FILE(const ResourceDirectory resourceDirectory, const char* fileName)
{
	return EOF;// fsDeleteFile(resourceDirectory, fileName) ? 0 : EOF;
}
#elif defined(_MSC_VER)
static FILE *miniz_fopen(const char *pFilename, const char *pMode) {
  FILE *pFile = NULL;
  fopen_s(&pFile, pFilename, pMode);
  return pFile;
}
static FILE *miniz_freopen(const char *pPath, const char *pMode, FILE *pStream) {
  FILE *pFile = NULL;
  if (freopen_s(&pFile, pPath, pMode, pStream))
    return NULL;
  return pFile;
}
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif
#define MINIZ_FILE FILE
#define MINIZ_FOPEN miniz_fopen
#define MINIZ_FCLOSE fclose
#define MINIZ_FREAD fread
#define MINIZ_FWRITE fwrite
#define MINIZ_FTELL64 _ftelli64
#define MINIZ_FSEEK64 _fseeki64
#define MINIZ_FILE_STAT_STRUCT _stat
#define MINIZ_FILE_STAT _stat
#define MINIZ_FFLUSH fflush
#define MINIZ_FREOPEN miniz_freopen
#define MINIZ_DELETE_FILE remove
#elif defined(__MINGW32__)
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif
#define MINIZ_FILE FILE
#define MINIZ_FOPEN(f, m) fopen(f, m)
#define MINIZ_FCLOSE fclose
#define MINIZ_FREAD fread
#define MINIZ_FWRITE fwrite
#define MINIZ_FTELL64 ftell
#define MINIZ_FSEEK64 fseek
#define MINIZ_FILE_STAT_STRUCT _stat
#define MINIZ_FILE_STAT _stat
#define MINIZ_FFLUSH fflush
#define MINIZ_FREOPEN(f, m, s) freopen(f, m, s)
#define MINIZ_DELETE_FILE remove
#elif defined(__TINYC__)
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif
#define MINIZ_FILE FILE
#define MINIZ_FOPEN(f, m) fopen(f, m)
#define MINIZ_FCLOSE fclose
#define MINIZ_FREAD fread
#define MINIZ_FWRITE fwrite
#define MINIZ_FTELL64 ftell
#define MINIZ_FSEEK64 fseek
#define MINIZ_FILE_STAT_STRUCT stat
#define MINIZ_FILE_STAT stat
#define MINIZ_FFLUSH fflush
#define MINIZ_FREOPEN(f, m, s) freopen(f, m, s)
#define MINIZ_DELETE_FILE remove
#elif defined(__GNUC__) && _LARGEFILE64_SOURCE
#ifndef MINIZ_NO_TIME
#include <utime.h>
#endif
#define MINIZ_FILE FILE
#define MINIZ_FOPEN(f, m) fopen64(f, m)
#define MINIZ_FCLOSE fclose
#define MINIZ_FREAD fread
#define MINIZ_FWRITE fwrite
#define MINIZ_FTELL64 ftello64
#define MINIZ_FSEEK64 fseeko64
#define MINIZ_FILE_STAT_STRUCT stat64
#define MINIZ_FILE_STAT stat64
#define MINIZ_FFLUSH fflush
#define MINIZ_FREOPEN(p, m, s) freopen64(p, m, s)
#define MINIZ_DELETE_FILE remove
#else
#if !defined(ORBIS)
#ifndef MINIZ_NO_TIME
#include <utime.h>
#endif
#endif
#define MINIZ_FILE FILE
#define MINIZ_FOPEN(f, m) fopen(f, m)
#define MINIZ_FCLOSE fclose
#define MINIZ_FREAD fread
#define MINIZ_FWRITE fwrite
#if _FILE_OFFSET_BITS == 64 || _POSIX_C_SOURCE >= 200112L
#define MINIZ_FTELL64 ftello
#define MINIZ_FSEEK64 fseeko
#else
#define MINIZ_FTELL64 ftell
#define MINIZ_FSEEK64 fseek
#endif
#define MINIZ_FILE_STAT_STRUCT stat
#define MINIZ_FILE_STAT stat
#define MINIZ_FFLUSH fflush
#define MINIZ_FREOPEN(f, m, s) freopen(f, m, s)
#define MINIZ_DELETE_FILE remove
#endif // #ifdef _MSC_VER
#endif // #ifdef MINIZ_NO_STDIO

#define MINIZ_TOLOWER(c) ((((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c))

// Various ZIP archive enums. To completely avoid cross platform compiler
// alignment and platform endian issues, miniz.c doesn't use structs for any of
// this stuff.
enum {
  // ZIP archive identifiers and record sizes
  MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG = 0x06054b50,
  MINIZ_ZIP_CENTRAL_DIR_HEADER_SIG = 0x02014b50,
  MINIZ_ZIP_LOCAL_DIR_HEADER_SIG = 0x04034b50,
  MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE = 30,
  MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE = 46,
  MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE = 22,

  /* ZIP64 archive identifier and record sizes */
  MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIG = 0x06064b50,
  MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG = 0x07064b50,
  MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE = 56,
  MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE = 20,
  MINIZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID = 0x0001,
  MINIZ_ZIP_DATA_DESCRIPTOR_ID = 0x08074b50,
  MINIZ_ZIP_DATA_DESCRIPTER_SIZE64 = 24,
  MINIZ_ZIP_DATA_DESCRIPTER_SIZE32 = 16,

  // Central directory header record offsets
  MINIZ_ZIP_CDH_SIG_OFS = 0,
  MINIZ_ZIP_CDH_VERSION_MADE_BY_OFS = 4,
  MINIZ_ZIP_CDH_VERSION_NEEDED_OFS = 6,
  MINIZ_ZIP_CDH_BIT_FLAG_OFS = 8,
  MINIZ_ZIP_CDH_METHOD_OFS = 10,
  MINIZ_ZIP_CDH_FILE_TIME_OFS = 12,
  MINIZ_ZIP_CDH_FILE_DATE_OFS = 14,
  MINIZ_ZIP_CDH_CRC32_OFS = 16,
  MINIZ_ZIP_CDH_COMPRESSED_SIZE_OFS = 20,
  MINIZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS = 24,
  MINIZ_ZIP_CDH_FILENAME_LEN_OFS = 28,
  MINIZ_ZIP_CDH_EXTRA_LEN_OFS = 30,
  MINIZ_ZIP_CDH_COMMENT_LEN_OFS = 32,
  MINIZ_ZIP_CDH_DISK_START_OFS = 34,
  MINIZ_ZIP_CDH_INTERNAL_ATTR_OFS = 36,
  MINIZ_ZIP_CDH_EXTERNAL_ATTR_OFS = 38,
  MINIZ_ZIP_CDH_LOCAL_HEADER_OFS = 42,
  // Local directory header offsets
  MINIZ_ZIP_LDH_SIG_OFS = 0,
  MINIZ_ZIP_LDH_VERSION_NEEDED_OFS = 4,
  MINIZ_ZIP_LDH_BIT_FLAG_OFS = 6,
  MINIZ_ZIP_LDH_METHOD_OFS = 8,
  MINIZ_ZIP_LDH_FILE_TIME_OFS = 10,
  MINIZ_ZIP_LDH_FILE_DATE_OFS = 12,
  MINIZ_ZIP_LDH_CRC32_OFS = 14,
  MINIZ_ZIP_LDH_COMPRESSED_SIZE_OFS = 18,
  MINIZ_ZIP_LDH_DECOMPRESSED_SIZE_OFS = 22,
  MINIZ_ZIP_LDH_FILENAME_LEN_OFS = 26,
  MINIZ_ZIP_LDH_EXTRA_LEN_OFS = 28,
  // End of central directory offsets
  MINIZ_ZIP_ECDH_SIG_OFS = 0,
  MINIZ_ZIP_ECDH_NUM_THIS_DISK_OFS = 4,
  MINIZ_ZIP_ECDH_NUM_DISK_CDIR_OFS = 6,
  MINIZ_ZIP_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS = 8,
  MINIZ_ZIP_ECDH_CDIR_TOTAL_ENTRIES_OFS = 10,
  MINIZ_ZIP_ECDH_CDIR_SIZE_OFS = 12,
  MINIZ_ZIP_ECDH_CDIR_OFS_OFS = 16,
  MINIZ_ZIP_ECDH_COMMENT_SIZE_OFS = 20,

  /* ZIP64 End of central directory locator offsets */
  MINIZ_ZIP64_ECDL_SIG_OFS = 0,                    /* 4 bytes */
  MINIZ_ZIP64_ECDL_NUM_DISK_CDIR_OFS = 4,          /* 4 bytes */
  MINIZ_ZIP64_ECDL_REL_OFS_TO_ZIP64_ECDR_OFS = 8,  /* 8 bytes */
  MINIZ_ZIP64_ECDL_TOTAL_NUMBER_OF_DISKS_OFS = 16, /* 4 bytes */

  /* ZIP64 End of central directory header offsets */
  MINIZ_ZIP64_ECDH_SIG_OFS = 0,                       /* 4 bytes */
  MINIZ_ZIP64_ECDH_SIZE_OF_RECORD_OFS = 4,            /* 8 bytes */
  MINIZ_ZIP64_ECDH_VERSION_MADE_BY_OFS = 12,          /* 2 bytes */
  MINIZ_ZIP64_ECDH_VERSION_NEEDED_OFS = 14,           /* 2 bytes */
  MINIZ_ZIP64_ECDH_NUM_THIS_DISK_OFS = 16,            /* 4 bytes */
  MINIZ_ZIP64_ECDH_NUM_DISK_CDIR_OFS = 20,            /* 4 bytes */
  MINIZ_ZIP64_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS = 24, /* 8 bytes */
  MINIZ_ZIP64_ECDH_CDIR_TOTAL_ENTRIES_OFS = 32,       /* 8 bytes */
  MINIZ_ZIP64_ECDH_CDIR_SIZE_OFS = 40,                /* 8 bytes */
  MINIZ_ZIP64_ECDH_CDIR_OFS_OFS = 48,                 /* 8 bytes */
  MINIZ_ZIP_VERSION_MADE_BY_DOS_FILESYSTEM_ID = 0,
  MINIZ_ZIP_DOS_DIR_ATTRIBUTE_BITFLAG = 0x10,
  MINIZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED = 1,
  MINIZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_COMPRESSED_PATCH_FLAG = 32,
  MINIZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION = 64,
  MINIZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_LOCAL_DIR_IS_MASKED = 8192,
  MINIZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_UTF8 = 1 << 11
};

typedef struct {
  void *m_p;
  size_t m_size, m_capacity;
  miniz_uint m_element_size;
} miniz_zip_array;

struct miniz_zip_internal_state_tag {
  miniz_zip_array m_central_dir;
  miniz_zip_array m_central_dir_offsets;
  miniz_zip_array m_sorted_central_dir_offsets;

  /* The flags passed in when the archive is initially opened. */
  uint32_t m_init_flags;

  /* MINIZ_TRUE if the archive has a zip64 end of central directory headers, etc.
   */
  miniz_bool m_zip64;

  /* MINIZ_TRUE if we found zip64 extended info in the central directory (m_zip64
   * will also be slammed to true too, even if we didn't find a zip64 end of
   * central dir header, etc.) */
  miniz_bool m_zip64_has_extended_info_fields;

  /* These fields are used by the file, FILE, memory, and memory/heap read/write
   * helpers. */
  MINIZ_FILE m_pFile;
  miniz_uint64 m_file_archive_start_ofs;

  void *m_pMem;
  size_t m_mem_size;
  size_t m_mem_capacity;
};

#define MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(array_ptr, element_size)                 \
  (array_ptr)->m_element_size = element_size
#define MINIZ_ZIP_ARRAY_ELEMENT(array_ptr, element_type, index)                   \
  ((element_type *)((array_ptr)->m_p))[index]

static MINIZ_FORCEINLINE void miniz_zip_array_clear(miniz_zip_archive *pZip,
                                              miniz_zip_array *pArray) {
  pZip->m_pFree(pZip->m_pAlloc_opaque, pArray->m_p);
  memset(pArray, 0, sizeof(miniz_zip_array));
}

static miniz_bool miniz_zip_array_ensure_capacity(miniz_zip_archive *pZip,
                                            miniz_zip_array *pArray,
                                            size_t min_new_capacity,
                                            miniz_uint growing) {
  void *pNew_p;
  size_t new_capacity = min_new_capacity;
  MINIZ_ASSERT(pArray->m_element_size);
  if (pArray->m_capacity >= min_new_capacity)
    return MINIZ_TRUE;
  if (growing) {
    new_capacity = MINIZ_MAX(1, pArray->m_capacity);
    while (new_capacity < min_new_capacity)
      new_capacity *= 2;
  }
  if (NULL == (pNew_p = pZip->m_pRealloc(pZip->m_pAlloc_opaque, pArray->m_p,
                                         pArray->m_element_size, new_capacity)))
    return MINIZ_FALSE;
  pArray->m_p = pNew_p;
  pArray->m_capacity = new_capacity;
  return MINIZ_TRUE;
}

static MINIZ_FORCEINLINE miniz_bool miniz_zip_array_reserve(miniz_zip_archive *pZip,
                                                   miniz_zip_array *pArray,
                                                   size_t new_capacity,
                                                   miniz_uint growing) {
  if (new_capacity > pArray->m_capacity) {
    if (!miniz_zip_array_ensure_capacity(pZip, pArray, new_capacity, growing))
      return MINIZ_FALSE;
  }
  return MINIZ_TRUE;
}

static MINIZ_FORCEINLINE miniz_bool miniz_zip_array_resize(miniz_zip_archive *pZip,
                                                  miniz_zip_array *pArray,
                                                  size_t new_size,
                                                  miniz_uint growing) {
  if (new_size > pArray->m_capacity) {
    if (!miniz_zip_array_ensure_capacity(pZip, pArray, new_size, growing))
      return MINIZ_FALSE;
  }
  pArray->m_size = new_size;
  return MINIZ_TRUE;
}

static MINIZ_FORCEINLINE miniz_bool miniz_zip_array_ensure_room(miniz_zip_archive *pZip,
                                                       miniz_zip_array *pArray,
                                                       size_t n) {
  return miniz_zip_array_reserve(pZip, pArray, pArray->m_size + n, MINIZ_TRUE);
}

static MINIZ_FORCEINLINE miniz_bool miniz_zip_array_push_back(miniz_zip_archive *pZip,
                                                     miniz_zip_array *pArray,
                                                     const void *pElements,
                                                     size_t n) {
  if (0 == n)
    return MINIZ_TRUE;
  if (!pElements)
    return MINIZ_FALSE;

  size_t orig_size = pArray->m_size;
  if (!miniz_zip_array_resize(pZip, pArray, orig_size + n, MINIZ_TRUE))
    return MINIZ_FALSE;
  memcpy((miniz_uint8 *)pArray->m_p + orig_size * pArray->m_element_size,
         pElements, n * pArray->m_element_size);
  return MINIZ_TRUE;
}

#ifndef MINIZ_NO_TIME
static time_t miniz_zip_dos_to_time_t(int dos_time, int dos_date) {
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  tm.tm_isdst = -1;
  tm.tm_year = ((dos_date >> 9) & 127) + 1980 - 1900;
  tm.tm_mon = ((dos_date >> 5) & 15) - 1;
  tm.tm_mday = dos_date & 31;
  tm.tm_hour = (dos_time >> 11) & 31;
  tm.tm_min = (dos_time >> 5) & 63;
  tm.tm_sec = (dos_time << 1) & 62;
  return mktime(&tm);
}

#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
static void miniz_zip_time_t_to_dos_time(time_t time, miniz_uint16 *pDOS_time,
                                      miniz_uint16 *pDOS_date) {
#ifdef _MSC_VER
  struct tm tm_struct;
  struct tm *tm = &tm_struct;
  errno_t err = localtime_s(tm, &time);
  if (err) {
    *pDOS_date = 0;
    *pDOS_time = 0;
    return;
  }
#else
  struct tm *tm = localtime(&time);
#endif /* #ifdef _MSC_VER */

  *pDOS_time = (miniz_uint16)(((tm->tm_hour) << 11) + ((tm->tm_min) << 5) +
                           ((tm->tm_sec) >> 1));
  *pDOS_date = (miniz_uint16)(((tm->tm_year + 1900 - 1980) << 9) +
                           ((tm->tm_mon + 1) << 5) + tm->tm_mday);
}
#endif /* MINIZ_NO_ARCHIVE_WRITING_APIS */

#ifndef MINIZ_NO_STDIO
#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
// CONFFX_BEGIN - Custom File IO
static miniz_bool miniz_zip_get_file_modified_time(const ResourceDirectory resourceDirectory, const char* fileName,
                                             time_t *pTime) {
	*pTime = fsGetLastModifiedTime(resourceDirectory, fileName);
	return MINIZ_TRUE;
}
// CONFFX_END
#endif /* #ifndef MINIZ_NO_ARCHIVE_WRITING_APIS*/

// CONFFX_BEGIN - Custom File IO
static miniz_bool miniz_zip_set_file_times(const ResourceDirectory resourceDirectory, const char* fileName, time_t access_time,
                                     time_t modified_time) {
	// #TODO
	UNREF_PARAM(resourceDirectory);
	UNREF_PARAM(fileName);
	UNREF_PARAM(access_time);
	UNREF_PARAM(modified_time);
	return MINIZ_TRUE;
//#if !defined(ORBIS)
//  struct utimbuf t;
//
//  memset(&t, 0, sizeof(t));
//  t.actime = access_time;
//  t.modtime = modified_time;
//
//  return !utime(pFilename, &t);
//// CONFFX_END
//#else
//	return MINIZ_FALSE;
//#endif
}
// CONFFX_END
#endif /* #ifndef MINIZ_NO_STDIO */
#endif /* #ifndef MINIZ_NO_TIME */

static MINIZ_FORCEINLINE miniz_bool miniz_zip_set_error(miniz_zip_archive *pZip,
                                               miniz_zip_error err_num) {
  if (pZip)
    pZip->m_last_error = err_num;
  return MINIZ_FALSE;
}

static miniz_bool miniz_zip_reader_init_internal(miniz_zip_archive *pZip,
                                           miniz_uint32 flags) {
  (void)flags;
  if ((!pZip) || (pZip->m_pState) || (pZip->m_zip_mode != MINIZ_ZIP_MODE_INVALID))
    return MINIZ_FALSE;

  if (!pZip->m_pAlloc)
    pZip->m_pAlloc = def_alloc_func;
  if (!pZip->m_pFree)
    pZip->m_pFree = def_free_func;
  if (!pZip->m_pRealloc)
    pZip->m_pRealloc = def_realloc_func;

  pZip->m_zip_mode = MINIZ_ZIP_MODE_READING;
  pZip->m_archive_size = 0;
  pZip->m_central_directory_file_ofs = 0;
  pZip->m_total_files = 0;

  if (NULL == (pZip->m_pState = (miniz_zip_internal_state *)pZip->m_pAlloc(
                   pZip->m_pAlloc_opaque, 1, sizeof(miniz_zip_internal_state))))
    return MINIZ_FALSE;
  memset(pZip->m_pState, 0, sizeof(miniz_zip_internal_state));
  MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir,
                                sizeof(miniz_uint8));
  MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir_offsets,
                                sizeof(miniz_uint32));
  MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_sorted_central_dir_offsets,
                                sizeof(miniz_uint32));
  return MINIZ_TRUE;
}

static MINIZ_FORCEINLINE miniz_bool
miniz_zip_reader_filename_less(const miniz_zip_array *pCentral_dir_array,
                            const miniz_zip_array *pCentral_dir_offsets,
                            miniz_uint l_index, miniz_uint r_index) {
  const miniz_uint8 *pL = &MINIZ_ZIP_ARRAY_ELEMENT(
                     pCentral_dir_array, miniz_uint8,
                     MINIZ_ZIP_ARRAY_ELEMENT(pCentral_dir_offsets, miniz_uint32,
                                          l_index)),
                 *pE;
  const miniz_uint8 *pR = &MINIZ_ZIP_ARRAY_ELEMENT(
      pCentral_dir_array, miniz_uint8,
      MINIZ_ZIP_ARRAY_ELEMENT(pCentral_dir_offsets, miniz_uint32, r_index));
  miniz_uint l_len = MINIZ_READ_LE16(pL + MINIZ_ZIP_CDH_FILENAME_LEN_OFS),
          r_len = MINIZ_READ_LE16(pR + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
  miniz_uint8 l = 0, r = 0;
  pL += MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
  pR += MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
  pE = pL + MINIZ_MIN(l_len, r_len);
  while (pL < pE) {
    if ((l = MINIZ_TOLOWER(*pL)) != (r = MINIZ_TOLOWER(*pR)))
      break;
    pL++;
    pR++;
  }
  return (pL == pE) ? (l_len < r_len) : (l < r);
}

#define MINIZ_SWAP_UINT32(a, b)                                                   \
  do {                                                                         \
    miniz_uint32 t = a;                                                           \
    a = b;                                                                     \
    b = t;                                                                     \
  }                                                                            \
  MINIZ_MACRO_END

// Heap sort of lowercased filenames, used to help accelerate plain central
// directory searches by miniz_zip_reader_locate_file(). (Could also use qsort(),
// but it could allocate memory.)
static void
miniz_zip_reader_sort_central_dir_offsets_by_filename(miniz_zip_archive *pZip) {
  miniz_zip_internal_state *pState = pZip->m_pState;
  const miniz_zip_array *pCentral_dir_offsets = &pState->m_central_dir_offsets;
  const miniz_zip_array *pCentral_dir = &pState->m_central_dir;
  miniz_uint32 *pIndices = &MINIZ_ZIP_ARRAY_ELEMENT(
      &pState->m_sorted_central_dir_offsets, miniz_uint32, 0);
  const int size = pZip->m_total_files;
  int start = (size - 2) >> 1, end;
  while (start >= 0) {
    int child, root = start;
    for (;;) {
      if ((child = (root << 1) + 1) >= size)
        break;
      child +=
          (((child + 1) < size) &&
           (miniz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets,
                                        pIndices[child], pIndices[child + 1])));
      if (!miniz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets,
                                       pIndices[root], pIndices[child]))
        break;
      MINIZ_SWAP_UINT32(pIndices[root], pIndices[child]);
      root = child;
    }
    start--;
  }

  end = size - 1;
  while (end > 0) {
    int child, root = 0;
    MINIZ_SWAP_UINT32(pIndices[end], pIndices[0]);
    for (;;) {
      if ((child = (root << 1) + 1) >= end)
        break;
      child +=
          (((child + 1) < end) &&
           miniz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets,
                                       pIndices[child], pIndices[child + 1]));
      if (!miniz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets,
                                       pIndices[root], pIndices[child]))
        break;
      MINIZ_SWAP_UINT32(pIndices[root], pIndices[child]);
      root = child;
    }
    end--;
  }
}

static miniz_bool miniz_zip_reader_locate_header_sig(miniz_zip_archive *pZip,
                                               miniz_uint32 record_sig,
                                               miniz_uint32 record_size,
                                               miniz_int64 *pOfs) {
  miniz_int64 cur_file_ofs;
  miniz_uint32 buf_u32[4096 / sizeof(miniz_uint32)];
  miniz_uint8 *pBuf = (miniz_uint8 *)buf_u32;

  /* Basic sanity checks - reject files which are too small */
  if (pZip->m_archive_size < record_size)
    return MINIZ_FALSE;

  /* Find the record by scanning the file from the end towards the beginning. */
  cur_file_ofs =
      MINIZ_MAX((miniz_int64)pZip->m_archive_size - (miniz_int64)sizeof(buf_u32), 0);
  for (;;) {
    int i,
        n = (int)MINIZ_MIN(sizeof(buf_u32), pZip->m_archive_size - cur_file_ofs);

    if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pBuf, n) != (miniz_uint)n)
      return MINIZ_FALSE;

    for (i = n - 4; i >= 0; --i) {
      miniz_uint s = MINIZ_READ_LE32(pBuf + i);
      if (s == record_sig) {
        if ((pZip->m_archive_size - (cur_file_ofs + i)) >= record_size)
          break;
      }
    }

    if (i >= 0) {
      cur_file_ofs += i;
      break;
    }

    /* Give up if we've searched the entire file, or we've gone back "too far"
     * (~64kb) */
    if ((!cur_file_ofs) || ((pZip->m_archive_size - cur_file_ofs) >=
                            (MINIZ_UINT16_MAX + record_size)))
      return MINIZ_FALSE;

    cur_file_ofs = MINIZ_MAX(cur_file_ofs - (sizeof(buf_u32) - 3), 0);
  }

  *pOfs = cur_file_ofs;
  return MINIZ_TRUE;
}

static miniz_bool miniz_zip_reader_read_central_dir(miniz_zip_archive *pZip,
                                              miniz_uint flags) {
  miniz_uint cdir_size = 0, cdir_entries_on_this_disk = 0, num_this_disk = 0,
          cdir_disk_index = 0;
  miniz_uint64 cdir_ofs = 0;
  miniz_int64 cur_file_ofs = 0;
  const miniz_uint8 *p;

  miniz_uint32 buf_u32[4096 / sizeof(miniz_uint32)];
  miniz_uint8 *pBuf = (miniz_uint8 *)buf_u32;
  miniz_bool sort_central_dir =
      ((flags & MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY) == 0);
  miniz_uint32 zip64_end_of_central_dir_locator_u32
      [(MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE + sizeof(miniz_uint32) - 1) /
       sizeof(miniz_uint32)];
  miniz_uint8 *pZip64_locator = (miniz_uint8 *)zip64_end_of_central_dir_locator_u32;

  miniz_uint32 zip64_end_of_central_dir_header_u32
      [(MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE + sizeof(miniz_uint32) - 1) /
       sizeof(miniz_uint32)];
  miniz_uint8 *pZip64_end_of_central_dir =
      (miniz_uint8 *)zip64_end_of_central_dir_header_u32;

  miniz_uint64 zip64_end_of_central_dir_ofs = 0;

  /* Basic sanity checks - reject files which are too small, and check the first
   * 4 bytes of the file to make sure a local header is there. */
  if (pZip->m_archive_size < MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
    return miniz_zip_set_error(pZip, MINIZ_ZIP_NOT_AN_ARCHIVE);

  if (!miniz_zip_reader_locate_header_sig(
          pZip, MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG,
          MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE, &cur_file_ofs))
    return miniz_zip_set_error(pZip, MINIZ_ZIP_FAILED_FINDING_CENTRAL_DIR);

  /* Read and verify the end of central directory record. */
  if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pBuf,
                    MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE) !=
      MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
    return miniz_zip_set_error(pZip, MINIZ_ZIP_FILE_READ_FAILED);

  if (MINIZ_READ_LE32(pBuf + MINIZ_ZIP_ECDH_SIG_OFS) !=
      MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG)
    return miniz_zip_set_error(pZip, MINIZ_ZIP_NOT_AN_ARCHIVE);

  if (cur_file_ofs >= (MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE +
                       MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE)) {
    if (pZip->m_pRead(pZip->m_pIO_opaque,
                      cur_file_ofs - MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE,
                      pZip64_locator,
                      MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE) ==
        MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE) {
      if (MINIZ_READ_LE32(pZip64_locator + MINIZ_ZIP64_ECDL_SIG_OFS) ==
          MINIZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG) {
        zip64_end_of_central_dir_ofs = MINIZ_READ_LE64(
            pZip64_locator + MINIZ_ZIP64_ECDL_REL_OFS_TO_ZIP64_ECDR_OFS);
        if (zip64_end_of_central_dir_ofs >
            (pZip->m_archive_size - MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE))
          return miniz_zip_set_error(pZip, MINIZ_ZIP_NOT_AN_ARCHIVE);

        if (pZip->m_pRead(pZip->m_pIO_opaque, zip64_end_of_central_dir_ofs,
                          pZip64_end_of_central_dir,
                          MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE) ==
            MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE) {
          if (MINIZ_READ_LE32(pZip64_end_of_central_dir + MINIZ_ZIP64_ECDH_SIG_OFS) ==
              MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIG) {
            pZip->m_pState->m_zip64 = MINIZ_TRUE;
          }
        }
      }
    }
  }

  pZip->m_total_files = MINIZ_READ_LE16(pBuf + MINIZ_ZIP_ECDH_CDIR_TOTAL_ENTRIES_OFS);
  cdir_entries_on_this_disk =
      MINIZ_READ_LE16(pBuf + MINIZ_ZIP_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS);
  num_this_disk = MINIZ_READ_LE16(pBuf + MINIZ_ZIP_ECDH_NUM_THIS_DISK_OFS);
  cdir_disk_index = MINIZ_READ_LE16(pBuf + MINIZ_ZIP_ECDH_NUM_DISK_CDIR_OFS);
  cdir_size = MINIZ_READ_LE32(pBuf + MINIZ_ZIP_ECDH_CDIR_SIZE_OFS);
  cdir_ofs = MINIZ_READ_LE32(pBuf + MINIZ_ZIP_ECDH_CDIR_OFS_OFS);

  if (pZip->m_pState->m_zip64) {
    miniz_uint32 zip64_total_num_of_disks =
        MINIZ_READ_LE32(pZip64_locator + MINIZ_ZIP64_ECDL_TOTAL_NUMBER_OF_DISKS_OFS);
    miniz_uint64 zip64_cdir_total_entries = MINIZ_READ_LE64(
        pZip64_end_of_central_dir + MINIZ_ZIP64_ECDH_CDIR_TOTAL_ENTRIES_OFS);
    miniz_uint64 zip64_cdir_total_entries_on_this_disk = MINIZ_READ_LE64(
        pZip64_end_of_central_dir + MINIZ_ZIP64_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS);
    miniz_uint64 zip64_size_of_end_of_central_dir_record = MINIZ_READ_LE64(
        pZip64_end_of_central_dir + MINIZ_ZIP64_ECDH_SIZE_OF_RECORD_OFS);
    miniz_uint64 zip64_size_of_central_directory =
        MINIZ_READ_LE64(pZip64_end_of_central_dir + MINIZ_ZIP64_ECDH_CDIR_SIZE_OFS);

    if (zip64_size_of_end_of_central_dir_record <
        (MINIZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE - 12))
      return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);

    if (zip64_total_num_of_disks != 1U)
      return miniz_zip_set_error(pZip, MINIZ_ZIP_UNSUPPORTED_MULTIDISK);

    /* Check for miniz's practical limits */
    if (zip64_cdir_total_entries > MINIZ_UINT32_MAX)
      return miniz_zip_set_error(pZip, MINIZ_ZIP_TOO_MANY_FILES);

    pZip->m_total_files = (miniz_uint32)zip64_cdir_total_entries;

    if (zip64_cdir_total_entries_on_this_disk > MINIZ_UINT32_MAX)
      return miniz_zip_set_error(pZip, MINIZ_ZIP_TOO_MANY_FILES);

    cdir_entries_on_this_disk =
        (miniz_uint32)zip64_cdir_total_entries_on_this_disk;

    /* Check for miniz's current practical limits (sorry, this should be enough
     * for millions of files) */
    if (zip64_size_of_central_directory > MINIZ_UINT32_MAX)
      return miniz_zip_set_error(pZip, MINIZ_ZIP_UNSUPPORTED_CDIR_SIZE);

    cdir_size = (miniz_uint32)zip64_size_of_central_directory;

    num_this_disk = MINIZ_READ_LE32(pZip64_end_of_central_dir +
                                 MINIZ_ZIP64_ECDH_NUM_THIS_DISK_OFS);

    cdir_disk_index = MINIZ_READ_LE32(pZip64_end_of_central_dir +
                                   MINIZ_ZIP64_ECDH_NUM_DISK_CDIR_OFS);

    cdir_ofs =
        MINIZ_READ_LE64(pZip64_end_of_central_dir + MINIZ_ZIP64_ECDH_CDIR_OFS_OFS);
  }

  if (pZip->m_total_files != cdir_entries_on_this_disk)
    return miniz_zip_set_error(pZip, MINIZ_ZIP_UNSUPPORTED_MULTIDISK);

  if (((num_this_disk | cdir_disk_index) != 0) &&
      ((num_this_disk != 1) || (cdir_disk_index != 1)))
    return miniz_zip_set_error(pZip, MINIZ_ZIP_UNSUPPORTED_MULTIDISK);

  if (cdir_size < pZip->m_total_files * MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE)
    return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);

  if ((cdir_ofs + (miniz_uint64)cdir_size) > pZip->m_archive_size)
    return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);

  pZip->m_central_directory_file_ofs = cdir_ofs;

  if (pZip->m_total_files) {
    miniz_uint i, n;
    /* Read the entire central directory into a heap block, and allocate another
     * heap block to hold the unsorted central dir file record offsets, and
     * possibly another to hold the sorted indices. */
    if ((!miniz_zip_array_resize(pZip, &pZip->m_pState->m_central_dir, cdir_size,
                              MINIZ_FALSE)) ||
        (!miniz_zip_array_resize(pZip, &pZip->m_pState->m_central_dir_offsets,
                              pZip->m_total_files, MINIZ_FALSE)))
      return miniz_zip_set_error(pZip, MINIZ_ZIP_ALLOC_FAILED);

    if (sort_central_dir) {
      if (!miniz_zip_array_resize(pZip,
                               &pZip->m_pState->m_sorted_central_dir_offsets,
                               pZip->m_total_files, MINIZ_FALSE))
        return miniz_zip_set_error(pZip, MINIZ_ZIP_ALLOC_FAILED);
    }

    if (pZip->m_pRead(pZip->m_pIO_opaque, cdir_ofs,
                      pZip->m_pState->m_central_dir.m_p,
                      cdir_size) != cdir_size)
      return miniz_zip_set_error(pZip, MINIZ_ZIP_FILE_READ_FAILED);

    /* Now create an index into the central directory file records, do some
     * basic sanity checking on each record */
    p = (const miniz_uint8 *)pZip->m_pState->m_central_dir.m_p;
    for (n = cdir_size, i = 0; i < pZip->m_total_files; ++i) {
      miniz_uint total_header_size, disk_index, bit_flags, filename_size,
          ext_data_size;
      miniz_uint64 comp_size, decomp_size, local_header_ofs;

      if ((n < MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE) ||
          (MINIZ_READ_LE32(p) != MINIZ_ZIP_CENTRAL_DIR_HEADER_SIG))
        return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);

      MINIZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, miniz_uint32,
                           i) =
          (miniz_uint32)(p - (const miniz_uint8 *)pZip->m_pState->m_central_dir.m_p);

      if (sort_central_dir)
        MINIZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_sorted_central_dir_offsets,
                             miniz_uint32, i) = i;

      comp_size = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_COMPRESSED_SIZE_OFS);
      decomp_size = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS);
      local_header_ofs = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_LOCAL_HEADER_OFS);
      filename_size = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
      ext_data_size = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_EXTRA_LEN_OFS);

      if ((!pZip->m_pState->m_zip64_has_extended_info_fields) &&
          (ext_data_size) &&
          (MINIZ_MAX(MINIZ_MAX(comp_size, decomp_size), local_header_ofs) ==
           MINIZ_UINT32_MAX)) {
        /* Attempt to find zip64 extended information field in the entry's extra
         * data */
        miniz_uint32 extra_size_remaining = ext_data_size;

        if (extra_size_remaining) {
          const miniz_uint8 *pExtra_data;
          void *buf = NULL;

          if (MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size + ext_data_size >
              n) {
            buf = MINIZ_MALLOC(ext_data_size);
            if (buf == NULL)
              return miniz_zip_set_error(pZip, MINIZ_ZIP_ALLOC_FAILED);

            if (pZip->m_pRead(pZip->m_pIO_opaque,
                              cdir_ofs + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
                                  filename_size,
                              buf, ext_data_size) != ext_data_size) {
              MINIZ_FREE(buf);
              return miniz_zip_set_error(pZip, MINIZ_ZIP_FILE_READ_FAILED);
            }

            pExtra_data = (miniz_uint8 *)buf;
          } else {
            pExtra_data = p + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size;
          }

          do {
            miniz_uint32 field_id;
            miniz_uint32 field_data_size;

            if (extra_size_remaining < (sizeof(miniz_uint16) * 2)) {
              MINIZ_FREE(buf);
              return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            }

            field_id = MINIZ_READ_LE16(pExtra_data);
            field_data_size = MINIZ_READ_LE16(pExtra_data + sizeof(miniz_uint16));

            if ((field_data_size + sizeof(miniz_uint16) * 2) >
                extra_size_remaining) {
              MINIZ_FREE(buf);
              return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            }

            if (field_id == MINIZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID) {
              /* Ok, the archive didn't have any zip64 headers but it uses a
               * zip64 extended information field so mark it as zip64 anyway
               * (this can occur with infozip's zip util when it reads
               * compresses files from stdin). */
              pZip->m_pState->m_zip64 = MINIZ_TRUE;
              pZip->m_pState->m_zip64_has_extended_info_fields = MINIZ_TRUE;
              break;
            }

            pExtra_data += sizeof(miniz_uint16) * 2 + field_data_size;
            extra_size_remaining =
                extra_size_remaining - sizeof(miniz_uint16) * 2 - field_data_size;
          } while (extra_size_remaining);

          MINIZ_FREE(buf);
        }
      }

      /* I've seen archives that aren't marked as zip64 that uses zip64 ext
       * data, argh */
      if ((comp_size != MINIZ_UINT32_MAX) && (decomp_size != MINIZ_UINT32_MAX)) {
        if (((!MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_METHOD_OFS)) &&
             (decomp_size != comp_size)) ||
            (decomp_size && !comp_size))
          return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
      }

      disk_index = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_DISK_START_OFS);
      if ((disk_index == MINIZ_UINT16_MAX) ||
          ((disk_index != num_this_disk) && (disk_index != 1)))
        return miniz_zip_set_error(pZip, MINIZ_ZIP_UNSUPPORTED_MULTIDISK);

      if (comp_size != MINIZ_UINT32_MAX) {
        if (((miniz_uint64)MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_LOCAL_HEADER_OFS) +
             MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + comp_size) > pZip->m_archive_size)
          return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
      }

      bit_flags = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_BIT_FLAG_OFS);
      if (bit_flags & MINIZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_LOCAL_DIR_IS_MASKED)
        return miniz_zip_set_error(pZip, MINIZ_ZIP_UNSUPPORTED_ENCRYPTION);

      if ((total_header_size = MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
                               MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILENAME_LEN_OFS) +
                               MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_EXTRA_LEN_OFS) +
                               MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_COMMENT_LEN_OFS)) >
          n)
        return miniz_zip_set_error(pZip, MINIZ_ZIP_INVALID_HEADER_OR_CORRUPTED);

      n -= total_header_size;
      p += total_header_size;
    }
  }

  if (sort_central_dir)
    miniz_zip_reader_sort_central_dir_offsets_by_filename(pZip);

  return MINIZ_TRUE;
}

miniz_bool miniz_zip_reader_init(miniz_zip_archive *pZip, miniz_uint64 size,
                           miniz_uint32 flags) {
  if ((!pZip) || (!pZip->m_pRead))
    return MINIZ_FALSE;
  if (!miniz_zip_reader_init_internal(pZip, flags))
    return MINIZ_FALSE;
  pZip->m_archive_size = size;
  if (!miniz_zip_reader_read_central_dir(pZip, flags)) {
    miniz_zip_reader_end(pZip);
    return MINIZ_FALSE;
  }
  return MINIZ_TRUE;
}

static size_t miniz_zip_mem_read_func(void *pOpaque, miniz_uint64 file_ofs,
                                   void *pBuf, size_t n) {
  miniz_zip_archive *pZip = (miniz_zip_archive *)pOpaque;
  size_t s = (file_ofs >= pZip->m_archive_size)
                 ? 0
                 : (size_t)MINIZ_MIN(pZip->m_archive_size - file_ofs, n);
  memcpy(pBuf, (const miniz_uint8 *)pZip->m_pState->m_pMem + file_ofs, s);
  return s;
}

miniz_bool miniz_zip_reader_init_mem(miniz_zip_archive *pZip, const void *pMem,
                               size_t size, miniz_uint32 flags) {
  if (!miniz_zip_reader_init_internal(pZip, flags))
    return MINIZ_FALSE;
  pZip->m_archive_size = size;
  pZip->m_pRead = miniz_zip_mem_read_func;
  pZip->m_pIO_opaque = pZip;
#ifdef __cplusplus
  pZip->m_pState->m_pMem = const_cast<void *>(pMem);
#else
  pZip->m_pState->m_pMem = (void *)pMem;
#endif
  pZip->m_pState->m_mem_size = size;
  if (!miniz_zip_reader_read_central_dir(pZip, flags)) {
    miniz_zip_reader_end(pZip);
    return MINIZ_FALSE;
  }
  return MINIZ_TRUE;
}

#ifndef MINIZ_NO_STDIO
static size_t miniz_zip_file_read_func(void *pOpaque, miniz_uint64 file_ofs,
                                    void *pBuf, size_t n) {
  miniz_zip_archive *pZip = (miniz_zip_archive *)pOpaque;
  miniz_int64 cur_ofs = MINIZ_FTELL64(&pZip->m_pState->m_pFile);
  if (((miniz_int64)file_ofs < 0) ||
      (((cur_ofs != (miniz_int64)file_ofs)) &&
       (MINIZ_FSEEK64(&pZip->m_pState->m_pFile, (miniz_int64)file_ofs, SEEK_SET))))
    return 0;
  return MINIZ_FREAD(pBuf, 1, n, &pZip->m_pState->m_pFile);
}

// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_reader_init_file(miniz_zip_archive *pZip, const ResourceDirectory resourceDirectory, const char* fileName,
                                const char* filePassword, miniz_uint32 flags) {
  miniz_uint64 file_size;
  MINIZ_FILE pFile = (MINIZ_FILE){0};
  if (!MINIZ_FOPEN(resourceDirectory, fileName, filePassword, "rb", &pFile))
    return MINIZ_FALSE;
  if (MINIZ_FSEEK64(&pFile, 0, SEEK_END)) {
    MINIZ_FCLOSE(&pFile);
    return MINIZ_FALSE;
  }
  file_size = MINIZ_FTELL64(&pFile);
  if (!miniz_zip_reader_init_internal(pZip, flags)) {
    MINIZ_FCLOSE(&pFile);
    return MINIZ_FALSE;
  }
  pZip->m_pRead = miniz_zip_file_read_func;
  pZip->m_pIO_opaque = pZip;
  pZip->m_pState->m_pFile = pFile;
  pZip->m_archive_size = file_size;
  if (!miniz_zip_reader_read_central_dir(pZip, flags)) {
    miniz_zip_reader_end(pZip);
    return MINIZ_FALSE;
  }
  return MINIZ_TRUE;
}
#endif // #ifndef MINIZ_NO_STDIO

miniz_uint miniz_zip_reader_get_num_files(miniz_zip_archive *pZip) {
  return pZip ? pZip->m_total_files : 0;
}

static MINIZ_FORCEINLINE const miniz_uint8 *
miniz_zip_reader_get_cdh(miniz_zip_archive *pZip, miniz_uint file_index) {
  if ((!pZip) || (!pZip->m_pState) || (file_index >= pZip->m_total_files) ||
      (pZip->m_zip_mode != MINIZ_ZIP_MODE_READING))
    return NULL;
  return &MINIZ_ZIP_ARRAY_ELEMENT(
      &pZip->m_pState->m_central_dir, miniz_uint8,
      MINIZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, miniz_uint32,
                           file_index));
}

miniz_bool miniz_zip_reader_is_file_encrypted(miniz_zip_archive *pZip,
                                        miniz_uint file_index) {
  miniz_uint m_bit_flag;
  const miniz_uint8 *p = miniz_zip_reader_get_cdh(pZip, file_index);
  if (!p)
    return MINIZ_FALSE;
  m_bit_flag = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_BIT_FLAG_OFS);
  return (m_bit_flag & 1);
}

miniz_bool miniz_zip_reader_is_file_a_directory(miniz_zip_archive *pZip,
                                          miniz_uint file_index) {
  miniz_uint filename_len, external_attr;
  const miniz_uint8 *p = miniz_zip_reader_get_cdh(pZip, file_index);
  if (!p)
    return MINIZ_FALSE;

  // First see if the filename ends with a '/' character.
  filename_len = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
  if (filename_len) {
    if (*(p + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_len - 1) == '/')
      return MINIZ_TRUE;
  }

  // Bugfix: This code was also checking if the internal attribute was non-zero,
  // which wasn't correct. Most/all zip writers (hopefully) set DOS
  // file/directory attributes in the low 16-bits, so check for the DOS
  // directory flag and ignore the source OS ID in the created by field.
  // FIXME: Remove this check? Is it necessary - we already check the filename.
  external_attr = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_EXTERNAL_ATTR_OFS);
  if ((external_attr & 0x10) != 0)
    return MINIZ_TRUE;

  return MINIZ_FALSE;
}

miniz_bool miniz_zip_reader_file_stat(miniz_zip_archive *pZip, miniz_uint file_index,
                                miniz_zip_archive_file_stat *pStat) {
  miniz_uint n;
  const miniz_uint8 *p = miniz_zip_reader_get_cdh(pZip, file_index);
  if ((!p) || (!pStat))
    return MINIZ_FALSE;

  // Unpack the central directory record.
  pStat->m_file_index = file_index;
  pStat->m_central_dir_ofs = MINIZ_ZIP_ARRAY_ELEMENT(
      &pZip->m_pState->m_central_dir_offsets, miniz_uint32, file_index);
  pStat->m_version_made_by = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_VERSION_MADE_BY_OFS);
  pStat->m_version_needed = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_VERSION_NEEDED_OFS);
  pStat->m_bit_flag = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_BIT_FLAG_OFS);
  pStat->m_method = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_METHOD_OFS);
#ifndef MINIZ_NO_TIME
  pStat->m_time =
      miniz_zip_dos_to_time_t(MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILE_TIME_OFS),
                           MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILE_DATE_OFS));
#endif
  pStat->m_crc32 = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_CRC32_OFS);
  pStat->m_comp_size = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_COMPRESSED_SIZE_OFS);
  pStat->m_uncomp_size = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS);
  pStat->m_internal_attr = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_INTERNAL_ATTR_OFS);
  pStat->m_external_attr = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_EXTERNAL_ATTR_OFS);
  pStat->m_local_header_ofs = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_LOCAL_HEADER_OFS);

  // Copy as much of the filename and comment as possible.
  n = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
  n = MINIZ_MIN(n, MINIZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE - 1);
  memcpy(pStat->m_filename, p + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE, n);
  pStat->m_filename[n] = '\0';

  n = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_COMMENT_LEN_OFS);
  n = MINIZ_MIN(n, MINIZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE - 1);
  pStat->m_comment_size = n;
  memcpy(pStat->m_comment,
         p + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
             MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILENAME_LEN_OFS) +
             MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_EXTRA_LEN_OFS),
         n);
  pStat->m_comment[n] = '\0';

  return MINIZ_TRUE;
}

miniz_uint miniz_zip_reader_get_filename(miniz_zip_archive *pZip, miniz_uint file_index,
                                   char *pFilename, miniz_uint filename_buf_size) {
  miniz_uint n;
  const miniz_uint8 *p = miniz_zip_reader_get_cdh(pZip, file_index);
  if (!p) {
    if (filename_buf_size)
      pFilename[0] = '\0';
    return 0;
  }
  n = MINIZ_READ_LE16(p + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
  if (filename_buf_size) {
    n = MINIZ_MIN(n, filename_buf_size - 1);
    memcpy(pFilename, p + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE, n);
    pFilename[n] = '\0';
  }
  return n + 1;
}

static MINIZ_FORCEINLINE miniz_bool miniz_zip_reader_string_equal(const char *pA,
                                                         const char *pB,
                                                         miniz_uint len,
                                                         miniz_uint flags) {
  miniz_uint i;
  if (flags & MINIZ_ZIP_FLAG_CASE_SENSITIVE)
    return 0 == memcmp(pA, pB, len);
  for (i = 0; i < len; ++i)
    if (MINIZ_TOLOWER(pA[i]) != MINIZ_TOLOWER(pB[i]))
      return MINIZ_FALSE;
  return MINIZ_TRUE;
}

static MINIZ_FORCEINLINE int
miniz_zip_reader_filename_compare(const miniz_zip_array *pCentral_dir_array,
                               const miniz_zip_array *pCentral_dir_offsets,
                               miniz_uint l_index, const char *pR, miniz_uint r_len) {
  const miniz_uint8 *pL = &MINIZ_ZIP_ARRAY_ELEMENT(
                     pCentral_dir_array, miniz_uint8,
                     MINIZ_ZIP_ARRAY_ELEMENT(pCentral_dir_offsets, miniz_uint32,
                                          l_index)),
                 *pE;
  miniz_uint l_len = MINIZ_READ_LE16(pL + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
  miniz_uint8 l = 0, r = 0;
  pL += MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
  pE = pL + MINIZ_MIN(l_len, r_len);
  while (pL < pE) {
    if ((l = MINIZ_TOLOWER(*pL)) != (r = MINIZ_TOLOWER(*pR)))
      break;
    pL++;
    pR++;
  }
  return (pL == pE) ? (int)(l_len - r_len) : (l - r);
}

static int miniz_zip_reader_locate_file_binary_search(miniz_zip_archive *pZip,
                                                   const char *pFilename) {
  miniz_zip_internal_state *pState = pZip->m_pState;
  const miniz_zip_array *pCentral_dir_offsets = &pState->m_central_dir_offsets;
  const miniz_zip_array *pCentral_dir = &pState->m_central_dir;
  miniz_uint32 *pIndices = &MINIZ_ZIP_ARRAY_ELEMENT(
      &pState->m_sorted_central_dir_offsets, miniz_uint32, 0);
  const int size = pZip->m_total_files;
  const miniz_uint filename_len = (miniz_uint)strlen(pFilename);
  int l = 0, h = size - 1;
  while (l <= h) {
    int m = (l + h) >> 1, file_index = pIndices[m],
        comp =
            miniz_zip_reader_filename_compare(pCentral_dir, pCentral_dir_offsets,
                                           file_index, pFilename, filename_len);
    if (!comp)
      return file_index;
    else if (comp < 0)
      l = m + 1;
    else
      h = m - 1;
  }
  return -1;
}

int miniz_zip_reader_locate_file(miniz_zip_archive *pZip, const char *pName,
                              const char *pComment, miniz_uint flags) {
  miniz_uint file_index;
  size_t name_len, comment_len;
  if ((!pZip) || (!pZip->m_pState) || (!pName) ||
      (pZip->m_zip_mode != MINIZ_ZIP_MODE_READING))
    return -1;
  if (((flags & (MINIZ_ZIP_FLAG_IGNORE_PATH | MINIZ_ZIP_FLAG_CASE_SENSITIVE)) == 0) &&
      (!pComment) && (pZip->m_pState->m_sorted_central_dir_offsets.m_size))
    return miniz_zip_reader_locate_file_binary_search(pZip, pName);
  name_len = strlen(pName);
  if (name_len > 0xFFFF)
    return -1;
  comment_len = pComment ? strlen(pComment) : 0;
  if (comment_len > 0xFFFF)
    return -1;
  for (file_index = 0; file_index < pZip->m_total_files; file_index++) {
    const miniz_uint8 *pHeader = &MINIZ_ZIP_ARRAY_ELEMENT(
        &pZip->m_pState->m_central_dir, miniz_uint8,
        MINIZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, miniz_uint32,
                             file_index));
    miniz_uint filename_len = MINIZ_READ_LE16(pHeader + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
    const char *pFilename =
        (const char *)pHeader + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
    if (filename_len < name_len)
      continue;
    if (comment_len) {
      miniz_uint file_extra_len = MINIZ_READ_LE16(pHeader + MINIZ_ZIP_CDH_EXTRA_LEN_OFS),
              file_comment_len =
                  MINIZ_READ_LE16(pHeader + MINIZ_ZIP_CDH_COMMENT_LEN_OFS);
      const char *pFile_comment = pFilename + filename_len + file_extra_len;
      if ((file_comment_len != comment_len) ||
          (!miniz_zip_reader_string_equal(pComment, pFile_comment,
                                       file_comment_len, flags)))
        continue;
    }
    if ((flags & MINIZ_ZIP_FLAG_IGNORE_PATH) && (filename_len)) {
      int ofs = filename_len - 1;
      do {
        if ((pFilename[ofs] == '/') || (pFilename[ofs] == '\\') ||
            (pFilename[ofs] == ':'))
          break;
      } while (--ofs >= 0);
      ofs++;
      pFilename += ofs;
      filename_len -= ofs;
    }
    if ((filename_len == name_len) &&
        (miniz_zip_reader_string_equal(pName, pFilename, filename_len, flags)))
      return file_index;
  }
  return -1;
}

miniz_bool miniz_zip_reader_extract_to_mem_no_alloc(miniz_zip_archive *pZip,
                                              miniz_uint file_index, void *pBuf,
                                              size_t buf_size, miniz_uint flags,
                                              void *pUser_read_buf,
                                              size_t user_read_buf_size) {
  int status = TINFL_STATUS_DONE;
  miniz_uint64 needed_size, cur_file_ofs, comp_remaining,
      out_buf_ofs = 0, read_buf_size, read_buf_ofs = 0, read_buf_avail;
  miniz_zip_archive_file_stat file_stat;
  void *pRead_buf;
  miniz_uint32
      local_header_u32[(MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(miniz_uint32) - 1) /
                       sizeof(miniz_uint32)];
  miniz_uint8 *pLocal_header = (miniz_uint8 *)local_header_u32;
  tinfl_decompressor inflator;

  if ((buf_size) && (!pBuf))
    return MINIZ_FALSE;

  if (!miniz_zip_reader_file_stat(pZip, file_index, &file_stat))
    return MINIZ_FALSE;

  // Empty file, or a directory (but not always a directory - I've seen odd zips
  // with directories that have compressed data which inflates to 0 bytes)
  if (!file_stat.m_comp_size)
    return MINIZ_TRUE;

  // Entry is a subdirectory (I've seen old zips with dir entries which have
  // compressed deflate data which inflates to 0 bytes, but these entries claim
  // to uncompress to 512 bytes in the headers). I'm torn how to handle this
  // case - should it fail instead?
  if (miniz_zip_reader_is_file_a_directory(pZip, file_index))
    return MINIZ_TRUE;

  // Encryption and patch files are not supported.
  if (file_stat.m_bit_flag & (1 | 32))
    return MINIZ_FALSE;

  // This function only supports stored and deflate.
  if ((!(flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA)) && (file_stat.m_method != 0) &&
      (file_stat.m_method != MINIZ_DEFLATED))
    return MINIZ_FALSE;

  // Ensure supplied output buffer is large enough.
  needed_size = (flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA) ? file_stat.m_comp_size
                                                      : file_stat.m_uncomp_size;
  if (buf_size < needed_size)
    return MINIZ_FALSE;

  // Read and parse the local directory entry.
  cur_file_ofs = file_stat.m_local_header_ofs;
  if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pLocal_header,
                    MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE) !=
      MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE)
    return MINIZ_FALSE;
  if (MINIZ_READ_LE32(pLocal_header) != MINIZ_ZIP_LOCAL_DIR_HEADER_SIG)
    return MINIZ_FALSE;

  cur_file_ofs += MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE +
                  MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_FILENAME_LEN_OFS) +
                  MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_EXTRA_LEN_OFS);
  if ((cur_file_ofs + file_stat.m_comp_size) > pZip->m_archive_size)
    return MINIZ_FALSE;

  if ((flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA) || (!file_stat.m_method)) {
    // The file is stored or the caller has requested the compressed data.
    if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pBuf,
                      (size_t)needed_size) != needed_size)
      return MINIZ_FALSE;
    return ((flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA) != 0) ||
           (miniz_crc32(MINIZ_CRC32_INIT, (const miniz_uint8 *)pBuf,
                     (size_t)file_stat.m_uncomp_size) == file_stat.m_crc32);
  }

  // Decompress the file either directly from memory or from a file input
  // buffer.
  tinfl_init(&inflator);

  if (pZip->m_pState->m_pMem) {
    // Read directly from the archive in memory.
    pRead_buf = (miniz_uint8 *)pZip->m_pState->m_pMem + cur_file_ofs;
    read_buf_size = read_buf_avail = file_stat.m_comp_size;
    comp_remaining = 0;
  } else if (pUser_read_buf) {
    // Use a user provided read buffer.
    if (!user_read_buf_size)
      return MINIZ_FALSE;
    pRead_buf = (miniz_uint8 *)pUser_read_buf;
    read_buf_size = user_read_buf_size;
    read_buf_avail = 0;
    comp_remaining = file_stat.m_comp_size;
  } else {
    // Temporarily allocate a read buffer.
    read_buf_size = MINIZ_MIN(file_stat.m_comp_size, MINIZ_ZIP_MAX_IO_BUF_SIZE);
#ifdef _MSC_VER
    if (((0, sizeof(size_t) == sizeof(miniz_uint32))) &&
        (read_buf_size > 0x7FFFFFFF))
#else
    if (((sizeof(size_t) == sizeof(miniz_uint32))) && (read_buf_size > 0x7FFFFFFF))
#endif
      return MINIZ_FALSE;
    if (NULL == (pRead_buf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1,
                                            (size_t)read_buf_size)))
      return MINIZ_FALSE;
    read_buf_avail = 0;
    comp_remaining = file_stat.m_comp_size;
  }

  do {
    size_t in_buf_size,
        out_buf_size = (size_t)(file_stat.m_uncomp_size - out_buf_ofs);
    if ((!read_buf_avail) && (!pZip->m_pState->m_pMem)) {
      read_buf_avail = MINIZ_MIN(read_buf_size, comp_remaining);
      if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pRead_buf,
                        (size_t)read_buf_avail) != read_buf_avail) {
        status = TINFL_STATUS_FAILED;
        break;
      }
      cur_file_ofs += read_buf_avail;
      comp_remaining -= read_buf_avail;
      read_buf_ofs = 0;
    }
    in_buf_size = (size_t)read_buf_avail;
    status = tinfl_decompress(
        &inflator, (miniz_uint8 *)pRead_buf + read_buf_ofs, &in_buf_size,
        (miniz_uint8 *)pBuf, (miniz_uint8 *)pBuf + out_buf_ofs, &out_buf_size,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
            (comp_remaining ? TINFL_FLAG_HAS_MORE_INPUT : 0));
    read_buf_avail -= in_buf_size;
    read_buf_ofs += in_buf_size;
    out_buf_ofs += out_buf_size;
  } while (status == TINFL_STATUS_NEEDS_MORE_INPUT);

  if (status == TINFL_STATUS_DONE) {
    // Make sure the entire file was decompressed, and check its CRC.
    if ((out_buf_ofs != file_stat.m_uncomp_size) ||
        (miniz_crc32(MINIZ_CRC32_INIT, (const miniz_uint8 *)pBuf,
                  (size_t)file_stat.m_uncomp_size) != file_stat.m_crc32))
      status = TINFL_STATUS_FAILED;
  }

  if ((!pZip->m_pState->m_pMem) && (!pUser_read_buf))
    pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);

  return status == TINFL_STATUS_DONE;
}

miniz_bool miniz_zip_reader_extract_file_to_mem_no_alloc(
    miniz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size,
    miniz_uint flags, void *pUser_read_buf, size_t user_read_buf_size) {
  int file_index = miniz_zip_reader_locate_file(pZip, pFilename, NULL, flags);
  if (file_index < 0)
    return MINIZ_FALSE;
  return miniz_zip_reader_extract_to_mem_no_alloc(pZip, file_index, pBuf, buf_size,
                                               flags, pUser_read_buf,
                                               user_read_buf_size);
}

miniz_bool miniz_zip_reader_extract_to_mem(miniz_zip_archive *pZip, miniz_uint file_index,
                                     void *pBuf, size_t buf_size,
                                     miniz_uint flags) {
  return miniz_zip_reader_extract_to_mem_no_alloc(pZip, file_index, pBuf, buf_size,
                                               flags, NULL, 0);
}

miniz_bool miniz_zip_reader_extract_file_to_mem(miniz_zip_archive *pZip,
                                          const char *pFilename, void *pBuf,
                                          size_t buf_size, miniz_uint flags) {
  return miniz_zip_reader_extract_file_to_mem_no_alloc(pZip, pFilename, pBuf,
                                                    buf_size, flags, NULL, 0);
}

void *miniz_zip_reader_extract_to_heap(miniz_zip_archive *pZip, miniz_uint file_index,
                                    size_t *pSize, miniz_uint flags) {
  miniz_uint64 comp_size, uncomp_size, alloc_size;
  const miniz_uint8 *p = miniz_zip_reader_get_cdh(pZip, file_index);
  void *pBuf;

  if (pSize)
    *pSize = 0;
  if (!p)
    return NULL;

  comp_size = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_COMPRESSED_SIZE_OFS);
  uncomp_size = MINIZ_READ_LE32(p + MINIZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS);

  alloc_size = (flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA) ? comp_size : uncomp_size;
#ifdef _MSC_VER
  if (((0, sizeof(size_t) == sizeof(miniz_uint32))) && (alloc_size > 0x7FFFFFFF))
#else
  if (((sizeof(size_t) == sizeof(miniz_uint32))) && (alloc_size > 0x7FFFFFFF))
#endif
    return NULL;
  if (NULL ==
      (pBuf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, (size_t)alloc_size)))
    return NULL;

  if (!miniz_zip_reader_extract_to_mem(pZip, file_index, pBuf, (size_t)alloc_size,
                                    flags)) {
    pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
    return NULL;
  }

  if (pSize)
    *pSize = (size_t)alloc_size;
  return pBuf;
}

void *miniz_zip_reader_extract_file_to_heap(miniz_zip_archive *pZip,
                                         const char *pFilename, size_t *pSize,
                                         miniz_uint flags) {
  int file_index = miniz_zip_reader_locate_file(pZip, pFilename, NULL, flags);
  if (file_index < 0) {
    if (pSize)
      *pSize = 0;
    return MINIZ_FALSE;
  }
  return miniz_zip_reader_extract_to_heap(pZip, file_index, pSize, flags);
}

miniz_bool miniz_zip_reader_extract_to_callback(miniz_zip_archive *pZip,
                                          miniz_uint file_index,
                                          miniz_file_write_func pCallback,
                                          void *pOpaque, miniz_uint flags) {
  int status = TINFL_STATUS_DONE;
  miniz_uint file_crc32 = MINIZ_CRC32_INIT;
  miniz_uint64 read_buf_size, read_buf_ofs = 0, read_buf_avail, comp_remaining,
                           out_buf_ofs = 0, cur_file_ofs;
  miniz_zip_archive_file_stat file_stat;
  void *pRead_buf = NULL;
  void *pWrite_buf = NULL;
  miniz_uint32
      local_header_u32[(MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(miniz_uint32) - 1) /
                       sizeof(miniz_uint32)];
  miniz_uint8 *pLocal_header = (miniz_uint8 *)local_header_u32;

  if (!miniz_zip_reader_file_stat(pZip, file_index, &file_stat))
    return MINIZ_FALSE;

  // Empty file, or a directory (but not always a directory - I've seen odd zips
  // with directories that have compressed data which inflates to 0 bytes)
  if (!file_stat.m_comp_size)
    return MINIZ_TRUE;

  // Entry is a subdirectory (I've seen old zips with dir entries which have
  // compressed deflate data which inflates to 0 bytes, but these entries claim
  // to uncompress to 512 bytes in the headers). I'm torn how to handle this
  // case - should it fail instead?
  if (miniz_zip_reader_is_file_a_directory(pZip, file_index))
    return MINIZ_TRUE;

  // Encryption and patch files are not supported.
  if (file_stat.m_bit_flag & (1 | 32))
    return MINIZ_FALSE;

  // This function only supports stored and deflate.
  if ((!(flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA)) && (file_stat.m_method != 0) &&
      (file_stat.m_method != MINIZ_DEFLATED))
    return MINIZ_FALSE;

  // Read and parse the local directory entry.
  cur_file_ofs = file_stat.m_local_header_ofs;
  if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pLocal_header,
                    MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE) !=
      MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE)
    return MINIZ_FALSE;
  if (MINIZ_READ_LE32(pLocal_header) != MINIZ_ZIP_LOCAL_DIR_HEADER_SIG)
    return MINIZ_FALSE;

  cur_file_ofs += MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE +
                  MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_FILENAME_LEN_OFS) +
                  MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_EXTRA_LEN_OFS);
  if ((cur_file_ofs + file_stat.m_comp_size) > pZip->m_archive_size)
    return MINIZ_FALSE;

  // Decompress the file either directly from memory or from a file input
  // buffer.
  if (pZip->m_pState->m_pMem) {
    pRead_buf = (miniz_uint8 *)pZip->m_pState->m_pMem + cur_file_ofs;
    read_buf_size = read_buf_avail = file_stat.m_comp_size;
    comp_remaining = 0;
  } else {
    read_buf_size = MINIZ_MIN(file_stat.m_comp_size, MINIZ_ZIP_MAX_IO_BUF_SIZE);
    if (NULL == (pRead_buf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1,
                                            (size_t)read_buf_size)))
      return MINIZ_FALSE;
    read_buf_avail = 0;
    comp_remaining = file_stat.m_comp_size;
  }

  if ((flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA) || (!file_stat.m_method)) {
    // The file is stored or the caller has requested the compressed data.
    if (pZip->m_pState->m_pMem) {
#ifdef _MSC_VER
      if (((0, sizeof(size_t) == sizeof(miniz_uint32))) &&
          (file_stat.m_comp_size > 0xFFFFFFFF))
#else
      if (((sizeof(size_t) == sizeof(miniz_uint32))) &&
          (file_stat.m_comp_size > 0xFFFFFFFF))
#endif
        return MINIZ_FALSE;
      if (pCallback(pOpaque, out_buf_ofs, pRead_buf,
                    (size_t)file_stat.m_comp_size) != file_stat.m_comp_size)
        status = TINFL_STATUS_FAILED;
      else if (!(flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA))
        file_crc32 =
            (miniz_uint32)miniz_crc32(file_crc32, (const miniz_uint8 *)pRead_buf,
                                (size_t)file_stat.m_comp_size);
      // cur_file_ofs += file_stat.m_comp_size;
      out_buf_ofs += file_stat.m_comp_size;
      // comp_remaining = 0;
    } else {
      while (comp_remaining) {
        read_buf_avail = MINIZ_MIN(read_buf_size, comp_remaining);
        if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pRead_buf,
                          (size_t)read_buf_avail) != read_buf_avail) {
          status = TINFL_STATUS_FAILED;
          break;
        }

        if (!(flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA))
          file_crc32 = (miniz_uint32)miniz_crc32(
              file_crc32, (const miniz_uint8 *)pRead_buf, (size_t)read_buf_avail);

        if (pCallback(pOpaque, out_buf_ofs, pRead_buf,
                      (size_t)read_buf_avail) != read_buf_avail) {
          status = TINFL_STATUS_FAILED;
          break;
        }
        cur_file_ofs += read_buf_avail;
        out_buf_ofs += read_buf_avail;
        comp_remaining -= read_buf_avail;
      }
    }
  } else {
    tinfl_decompressor inflator;
    tinfl_init(&inflator);

    if (NULL == (pWrite_buf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1,
                                             TINFL_LZ_DICT_SIZE)))
      status = TINFL_STATUS_FAILED;
    else {
      do {
        miniz_uint8 *pWrite_buf_cur =
            (miniz_uint8 *)pWrite_buf + (out_buf_ofs & (TINFL_LZ_DICT_SIZE - 1));
        size_t in_buf_size,
            out_buf_size =
                TINFL_LZ_DICT_SIZE - (out_buf_ofs & (TINFL_LZ_DICT_SIZE - 1));
        if ((!read_buf_avail) && (!pZip->m_pState->m_pMem)) {
          read_buf_avail = MINIZ_MIN(read_buf_size, comp_remaining);
          if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pRead_buf,
                            (size_t)read_buf_avail) != read_buf_avail) {
            status = TINFL_STATUS_FAILED;
            break;
          }
          cur_file_ofs += read_buf_avail;
          comp_remaining -= read_buf_avail;
          read_buf_ofs = 0;
        }

        in_buf_size = (size_t)read_buf_avail;
        status = tinfl_decompress(
            &inflator, (const miniz_uint8 *)pRead_buf + read_buf_ofs, &in_buf_size,
            (miniz_uint8 *)pWrite_buf, pWrite_buf_cur, &out_buf_size,
            comp_remaining ? TINFL_FLAG_HAS_MORE_INPUT : 0);
        read_buf_avail -= in_buf_size;
        read_buf_ofs += in_buf_size;

        if (out_buf_size) {
          if (pCallback(pOpaque, out_buf_ofs, pWrite_buf_cur, out_buf_size) !=
              out_buf_size) {
            status = TINFL_STATUS_FAILED;
            break;
          }
          file_crc32 =
              (miniz_uint32)miniz_crc32(file_crc32, pWrite_buf_cur, out_buf_size);
          if ((out_buf_ofs += out_buf_size) > file_stat.m_uncomp_size) {
            status = TINFL_STATUS_FAILED;
            break;
          }
        }
      } while ((status == TINFL_STATUS_NEEDS_MORE_INPUT) ||
               (status == TINFL_STATUS_HAS_MORE_OUTPUT));
    }
  }

  if ((status == TINFL_STATUS_DONE) &&
      (!(flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA))) {
    // Make sure the entire file was decompressed, and check its CRC.
    if ((out_buf_ofs != file_stat.m_uncomp_size) ||
        (file_crc32 != file_stat.m_crc32))
      status = TINFL_STATUS_FAILED;
  }

  if (!pZip->m_pState->m_pMem)
    pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
  if (pWrite_buf)
    pZip->m_pFree(pZip->m_pAlloc_opaque, pWrite_buf);

  return status == TINFL_STATUS_DONE;
}

miniz_bool miniz_zip_reader_extract_file_to_callback(miniz_zip_archive *pZip,
                                               const char *pFilename,
                                               miniz_file_write_func pCallback,
                                               void *pOpaque, miniz_uint flags) {
  int file_index = miniz_zip_reader_locate_file(pZip, pFilename, NULL, flags);
  if (file_index < 0)
    return MINIZ_FALSE;
  return miniz_zip_reader_extract_to_callback(pZip, file_index, pCallback, pOpaque,
                                           flags);
}

#ifndef MINIZ_NO_STDIO
static size_t miniz_zip_file_write_callback(void *pOpaque, miniz_uint64 ofs,
                                         const void *pBuf, size_t n) {
  (void)ofs;
  return MINIZ_FWRITE(pBuf, 1, n, (MINIZ_FILE *)pOpaque);
}

// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_reader_extract_to_file(miniz_zip_archive *pZip, miniz_uint file_index,
	const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword,
                                      miniz_uint flags) {
  miniz_bool status;
  miniz_zip_archive_file_stat file_stat;
  MINIZ_FILE pFile;
  if (!miniz_zip_reader_file_stat(pZip, file_index, &file_stat))
    return MINIZ_FALSE;

  if (!MINIZ_FOPEN(resourceDirectory, fileName, "wb", filePassword, &pFile))
    return MINIZ_FALSE;
  status = miniz_zip_reader_extract_to_callback(
      pZip, file_index, miniz_zip_file_write_callback, &pFile, flags);
  if (MINIZ_FCLOSE(&pFile) == EOF)
    return MINIZ_FALSE;
#ifndef MINIZ_NO_TIME
  if (status) {
    miniz_zip_set_file_times(resourceDirectory, fileName, file_stat.m_time, file_stat.m_time);
  }
#endif

  return status;
}
#endif // #ifndef MINIZ_NO_STDIO

miniz_bool miniz_zip_reader_end(miniz_zip_archive *pZip) {
  if ((!pZip) || (!pZip->m_pState) || (!pZip->m_pAlloc) || (!pZip->m_pFree) ||
      (pZip->m_zip_mode != MINIZ_ZIP_MODE_READING))
    return MINIZ_FALSE;

  miniz_zip_internal_state *pState = pZip->m_pState;
  pZip->m_pState = NULL;
  miniz_zip_array_clear(pZip, &pState->m_central_dir);
  miniz_zip_array_clear(pZip, &pState->m_central_dir_offsets);
  miniz_zip_array_clear(pZip, &pState->m_sorted_central_dir_offsets);

#ifndef MINIZ_NO_STDIO
    MINIZ_FCLOSE(&pState->m_pFile);
#endif // #ifndef MINIZ_NO_STDIO

  pZip->m_pFree(pZip->m_pAlloc_opaque, pState);

  pZip->m_zip_mode = MINIZ_ZIP_MODE_INVALID;

  return MINIZ_TRUE;
}

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_reader_extract_file_to_file(miniz_zip_archive *pZip,
                                           const char *pArchive_filename,
                                           const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword,
                                           miniz_uint flags) {
  int file_index =
      miniz_zip_reader_locate_file(pZip, pArchive_filename, NULL, flags);
  if (file_index < 0)
    return MINIZ_FALSE;
  return miniz_zip_reader_extract_to_file(pZip, file_index, resourceDirectory, fileName, filePassword, flags);
}
#endif

// ------------------- .ZIP archive writing

#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS

static void miniz_write_le16(miniz_uint8 *p, miniz_uint16 v) {
  p[0] = (miniz_uint8)v;
  p[1] = (miniz_uint8)(v >> 8);
}
static void miniz_write_le32(miniz_uint8 *p, miniz_uint32 v) {
  p[0] = (miniz_uint8)v;
  p[1] = (miniz_uint8)(v >> 8);
  p[2] = (miniz_uint8)(v >> 16);
  p[3] = (miniz_uint8)(v >> 24);
}
#define MINIZ_WRITE_LE16(p, v) miniz_write_le16((miniz_uint8 *)(p), (miniz_uint16)(v))
#define MINIZ_WRITE_LE32(p, v) miniz_write_le32((miniz_uint8 *)(p), (miniz_uint32)(v))

miniz_bool miniz_zip_writer_init(miniz_zip_archive *pZip, miniz_uint64 existing_size) {
  if ((!pZip) || (pZip->m_pState) || (!pZip->m_pWrite) ||
      (pZip->m_zip_mode != MINIZ_ZIP_MODE_INVALID))
    return MINIZ_FALSE;

  if (pZip->m_file_offset_alignment) {
    // Ensure user specified file offset alignment is a power of 2.
    if (pZip->m_file_offset_alignment & (pZip->m_file_offset_alignment - 1))
      return MINIZ_FALSE;
  }

  if (!pZip->m_pAlloc)
    pZip->m_pAlloc = def_alloc_func;
  if (!pZip->m_pFree)
    pZip->m_pFree = def_free_func;
  if (!pZip->m_pRealloc)
    pZip->m_pRealloc = def_realloc_func;

  pZip->m_zip_mode = MINIZ_ZIP_MODE_WRITING;
  pZip->m_archive_size = existing_size;
  pZip->m_central_directory_file_ofs = 0;
  pZip->m_total_files = 0;

  if (NULL == (pZip->m_pState = (miniz_zip_internal_state *)pZip->m_pAlloc(
                   pZip->m_pAlloc_opaque, 1, sizeof(miniz_zip_internal_state))))
    return MINIZ_FALSE;
  memset(pZip->m_pState, 0, sizeof(miniz_zip_internal_state));
  MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir,
                                sizeof(miniz_uint8));
  MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir_offsets,
                                sizeof(miniz_uint32));
  MINIZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_sorted_central_dir_offsets,
                                sizeof(miniz_uint32));
  return MINIZ_TRUE;
}

static size_t miniz_zip_heap_write_func(void *pOpaque, miniz_uint64 file_ofs,
                                     const void *pBuf, size_t n) {
  miniz_zip_archive *pZip = (miniz_zip_archive *)pOpaque;
  miniz_zip_internal_state *pState = pZip->m_pState;
  miniz_uint64 new_size = MINIZ_MAX(file_ofs + n, pState->m_mem_size);

  if ((!n) ||
      ((sizeof(size_t) == sizeof(miniz_uint32)) && (new_size > 0x7FFFFFFF)))
    return 0;

  if (new_size > pState->m_mem_capacity) {
    void *pNew_block;
    size_t new_capacity = MINIZ_MAX(64, pState->m_mem_capacity);
    while (new_capacity < new_size)
      new_capacity *= 2;
    if (NULL == (pNew_block = pZip->m_pRealloc(
                     pZip->m_pAlloc_opaque, pState->m_pMem, 1, new_capacity)))
      return 0;
    pState->m_pMem = pNew_block;
    pState->m_mem_capacity = new_capacity;
  }
  memcpy((miniz_uint8 *)pState->m_pMem + file_ofs, pBuf, n);
  pState->m_mem_size = (size_t)new_size;
  return n;
}

miniz_bool miniz_zip_writer_init_heap(miniz_zip_archive *pZip,
                                size_t size_to_reserve_at_beginning,
                                size_t initial_allocation_size) {
  pZip->m_pWrite = miniz_zip_heap_write_func;
  pZip->m_pIO_opaque = pZip;
  if (!miniz_zip_writer_init(pZip, size_to_reserve_at_beginning))
    return MINIZ_FALSE;
  if (0 != (initial_allocation_size = MINIZ_MAX(initial_allocation_size,
                                             size_to_reserve_at_beginning))) {
    if (NULL == (pZip->m_pState->m_pMem = pZip->m_pAlloc(
                     pZip->m_pAlloc_opaque, 1, initial_allocation_size))) {
      miniz_zip_writer_end(pZip);
      return MINIZ_FALSE;
    }
    pZip->m_pState->m_mem_capacity = initial_allocation_size;
  }
  return MINIZ_TRUE;
}

#ifndef MINIZ_NO_STDIO
static size_t miniz_zip_file_write_func(void *pOpaque, miniz_uint64 file_ofs,
                                     const void *pBuf, size_t n) {
  miniz_zip_archive *pZip = (miniz_zip_archive *)pOpaque;
  miniz_int64 cur_ofs = MINIZ_FTELL64(&pZip->m_pState->m_pFile);
  if (((miniz_int64)file_ofs < 0) ||
      (((cur_ofs != (miniz_int64)file_ofs)) &&
       (MINIZ_FSEEK64(&pZip->m_pState->m_pFile, (miniz_int64)file_ofs, SEEK_SET))))
    return 0;
  return MINIZ_FWRITE(pBuf, 1, n, &pZip->m_pState->m_pFile);
}

// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_writer_init_file(miniz_zip_archive *pZip, const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword,
                                miniz_uint64 size_to_reserve_at_beginning) {
  MINIZ_FILE pFile;
  pZip->m_pWrite = miniz_zip_file_write_func;
  pZip->m_pIO_opaque = pZip;
  if (!miniz_zip_writer_init(pZip, size_to_reserve_at_beginning))
    return MINIZ_FALSE;
  if (!MINIZ_FOPEN(resourceDirectory, fileName, filePassword, "wb", &pFile)) {
    miniz_zip_writer_end(pZip);
    return MINIZ_FALSE;
  }
  pZip->m_pState->m_pFile = pFile;
  if (size_to_reserve_at_beginning) {
    miniz_uint64 cur_ofs = 0;
    char buf[4096];
    MINIZ_CLEAR_OBJ(buf);
    do {
      size_t n = (size_t)MINIZ_MIN(sizeof(buf), size_to_reserve_at_beginning);
      if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_ofs, buf, n) != n) {
        miniz_zip_writer_end(pZip);
        return MINIZ_FALSE;
      }
      cur_ofs += n;
      size_to_reserve_at_beginning -= n;
    } while (size_to_reserve_at_beginning);
  }
  return MINIZ_TRUE;
}
#endif // #ifndef MINIZ_NO_STDIO

// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_writer_init_from_reader(miniz_zip_archive *pZip,
                                       const ResourceDirectory resourceDirectory, const char* fileName) {
  miniz_zip_internal_state *pState;
  if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MINIZ_ZIP_MODE_READING))
    return MINIZ_FALSE;
  // No sense in trying to write to an archive that's already at the support max
  // size
  if ((pZip->m_total_files == 0xFFFF) ||
      ((pZip->m_archive_size + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
        MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE) > 0xFFFFFFFF))
    return MINIZ_FALSE;

  pState = pZip->m_pState;

  if (pState->m_pFile.pFile) {
#ifdef MINIZ_NO_STDIO
    pFilename;
    return MINIZ_FALSE;
#else
    // Archive is being read from stdio - try to reopen as writable.
    if (pZip->m_pIO_opaque != pZip)
      return MINIZ_FALSE;
    pZip->m_pWrite = miniz_zip_file_write_func;
    if (!MINIZ_FREOPEN(resourceDirectory, fileName, "r+b", &pState->m_pFile)) {
      // The miniz_zip_archive is now in a bogus state because pState->m_pFile is
      // NULL, so just close it.
      miniz_zip_reader_end(pZip);
      return MINIZ_FALSE;
    }
#endif // #ifdef MINIZ_NO_STDIO
  } else if (pState->m_pMem) {
    // Archive lives in a memory block. Assume it's from the heap that we can
    // resize using the realloc callback.
    if (pZip->m_pIO_opaque != pZip)
      return MINIZ_FALSE;
    pState->m_mem_capacity = pState->m_mem_size;
    pZip->m_pWrite = miniz_zip_heap_write_func;
  }
  // Archive is being read via a user provided read function - make sure the
  // user has specified a write function too.
  else if (!pZip->m_pWrite)
    return MINIZ_FALSE;

  // Start writing new files at the archive's current central directory
  // location.
  pZip->m_archive_size = pZip->m_central_directory_file_ofs;
  pZip->m_zip_mode = MINIZ_ZIP_MODE_WRITING;
  pZip->m_central_directory_file_ofs = 0;

  return MINIZ_TRUE;
}

miniz_bool miniz_zip_writer_add_mem(miniz_zip_archive *pZip, const char *pArchive_name,
                              const void *pBuf, size_t buf_size,
                              miniz_uint level_and_flags) {
  return miniz_zip_writer_add_mem_ex(pZip, pArchive_name, pBuf, buf_size, NULL, 0,
                                  level_and_flags, 0, 0);
}

typedef struct {
  miniz_zip_archive *m_pZip;
  miniz_uint64 m_cur_archive_file_ofs;
  miniz_uint64 m_comp_size;
} miniz_zip_writer_add_state;

static miniz_bool miniz_zip_writer_add_put_buf_callback(const void *pBuf, int len,
                                                  void *pUser) {
  miniz_zip_writer_add_state *pState = (miniz_zip_writer_add_state *)pUser;
  if ((int)pState->m_pZip->m_pWrite(pState->m_pZip->m_pIO_opaque,
                                    pState->m_cur_archive_file_ofs, pBuf,
                                    len) != len)
    return MINIZ_FALSE;
  pState->m_cur_archive_file_ofs += len;
  pState->m_comp_size += len;
  return MINIZ_TRUE;
}

static miniz_bool miniz_zip_writer_create_local_dir_header(
    miniz_zip_archive *pZip, miniz_uint8 *pDst, miniz_uint16 filename_size,
    miniz_uint16 extra_size, miniz_uint64 uncomp_size, miniz_uint64 comp_size,
    miniz_uint32 uncomp_crc32, miniz_uint16 method, miniz_uint16 bit_flags,
    miniz_uint16 dos_time, miniz_uint16 dos_date) {
  (void)pZip;
  memset(pDst, 0, MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_LDH_SIG_OFS, MINIZ_ZIP_LOCAL_DIR_HEADER_SIG);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_VERSION_NEEDED_OFS, method ? 20 : 0);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_BIT_FLAG_OFS, bit_flags);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_METHOD_OFS, method);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_FILE_TIME_OFS, dos_time);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_FILE_DATE_OFS, dos_date);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_LDH_CRC32_OFS, uncomp_crc32);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_LDH_COMPRESSED_SIZE_OFS, comp_size);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_LDH_DECOMPRESSED_SIZE_OFS, uncomp_size);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_FILENAME_LEN_OFS, filename_size);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_LDH_EXTRA_LEN_OFS, extra_size);
  return MINIZ_TRUE;
}

static miniz_bool miniz_zip_writer_create_central_dir_header(
    miniz_zip_archive *pZip, miniz_uint8 *pDst, miniz_uint16 filename_size,
    miniz_uint16 extra_size, miniz_uint16 comment_size, miniz_uint64 uncomp_size,
    miniz_uint64 comp_size, miniz_uint32 uncomp_crc32, miniz_uint16 method,
    miniz_uint16 bit_flags, miniz_uint16 dos_time, miniz_uint16 dos_date,
    miniz_uint64 local_header_ofs, miniz_uint32 ext_attributes) {
  (void)pZip;
  miniz_uint16 version_made_by = 10 * MINIZ_VER_MAJOR + MINIZ_VER_MINOR;
  version_made_by |= (MINIZ_PLATFORM << 8);

  memset(pDst, 0, MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_CDH_SIG_OFS, MINIZ_ZIP_CENTRAL_DIR_HEADER_SIG);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_VERSION_MADE_BY_OFS, version_made_by);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_VERSION_NEEDED_OFS, method ? 20 : 0);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_BIT_FLAG_OFS, bit_flags);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_METHOD_OFS, method);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_FILE_TIME_OFS, dos_time);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_FILE_DATE_OFS, dos_date);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_CDH_CRC32_OFS, uncomp_crc32);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_CDH_COMPRESSED_SIZE_OFS, comp_size);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS, uncomp_size);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_FILENAME_LEN_OFS, filename_size);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_EXTRA_LEN_OFS, extra_size);
  MINIZ_WRITE_LE16(pDst + MINIZ_ZIP_CDH_COMMENT_LEN_OFS, comment_size);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_CDH_EXTERNAL_ATTR_OFS, ext_attributes);
  MINIZ_WRITE_LE32(pDst + MINIZ_ZIP_CDH_LOCAL_HEADER_OFS, local_header_ofs);
  return MINIZ_TRUE;
}

static miniz_bool miniz_zip_writer_add_to_central_dir(
    miniz_zip_archive *pZip, const char *pFilename, miniz_uint16 filename_size,
    const void *pExtra, miniz_uint16 extra_size, const void *pComment,
    miniz_uint16 comment_size, miniz_uint64 uncomp_size, miniz_uint64 comp_size,
    miniz_uint32 uncomp_crc32, miniz_uint16 method, miniz_uint16 bit_flags,
    miniz_uint16 dos_time, miniz_uint16 dos_date, miniz_uint64 local_header_ofs,
    miniz_uint32 ext_attributes) {
  miniz_zip_internal_state *pState = pZip->m_pState;
  miniz_uint32 central_dir_ofs = (miniz_uint32)pState->m_central_dir.m_size;
  size_t orig_central_dir_size = pState->m_central_dir.m_size;
  miniz_uint8 central_dir_header[MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE];

  // No zip64 support yet
  if ((local_header_ofs > 0xFFFFFFFF) ||
      (((miniz_uint64)pState->m_central_dir.m_size +
        MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size + extra_size +
        comment_size) > 0xFFFFFFFF))
    return MINIZ_FALSE;

  if (!miniz_zip_writer_create_central_dir_header(
          pZip, central_dir_header, filename_size, extra_size, comment_size,
          uncomp_size, comp_size, uncomp_crc32, method, bit_flags, dos_time,
          dos_date, local_header_ofs, ext_attributes))
    return MINIZ_FALSE;

  if ((!miniz_zip_array_push_back(pZip, &pState->m_central_dir, central_dir_header,
                               MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE)) ||
      (!miniz_zip_array_push_back(pZip, &pState->m_central_dir, pFilename,
                               filename_size)) ||
      (!miniz_zip_array_push_back(pZip, &pState->m_central_dir, pExtra,
                               extra_size)) ||
      (!miniz_zip_array_push_back(pZip, &pState->m_central_dir, pComment,
                               comment_size)) ||
      (!miniz_zip_array_push_back(pZip, &pState->m_central_dir_offsets,
                               &central_dir_ofs, 1))) {
    // Try to push the central directory array back into its original state.
    miniz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size,
                        MINIZ_FALSE);
    return MINIZ_FALSE;
  }

  return MINIZ_TRUE;
}

static miniz_bool miniz_zip_writer_validate_archive_name(const char *pArchive_name) {
  // Basic ZIP archive filename validity checks: Valid filenames cannot start
  // with a forward slash, cannot contain a drive letter, and cannot use
  // DOS-style backward slashes.
  if (*pArchive_name == '/')
    return MINIZ_FALSE;
  while (*pArchive_name) {
    if ((*pArchive_name == '\\') || (*pArchive_name == ':'))
      return MINIZ_FALSE;
    pArchive_name++;
  }
  return MINIZ_TRUE;
}

static miniz_uint
miniz_zip_writer_compute_padding_needed_for_file_alignment(miniz_zip_archive *pZip) {
  miniz_uint32 n;
  if (!pZip->m_file_offset_alignment)
    return 0;
  n = (miniz_uint32)(pZip->m_archive_size & (pZip->m_file_offset_alignment - 1));
  return (miniz_uint)((pZip->m_file_offset_alignment - n) &
         (pZip->m_file_offset_alignment - 1));
}

static miniz_bool miniz_zip_writer_write_zeros(miniz_zip_archive *pZip,
                                         miniz_uint64 cur_file_ofs, miniz_uint32 n) {
  char buf[4096];
  memset(buf, 0, MINIZ_MIN(sizeof(buf), n));
  while (n) {
    miniz_uint32 s = MINIZ_MIN(sizeof(buf), n);
    if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_file_ofs, buf, s) != s)
      return MINIZ_FALSE;
    cur_file_ofs += s;
    n -= s;
  }
  return MINIZ_TRUE;
}

miniz_bool miniz_zip_writer_add_mem_ex(miniz_zip_archive *pZip,
                                 const char *pArchive_name, const void *pBuf,
                                 size_t buf_size, const void *pComment,
                                 miniz_uint16 comment_size,
                                 miniz_uint level_and_flags, miniz_uint64 uncomp_size,
                                 miniz_uint32 uncomp_crc32) {
  miniz_uint32 ext_attributes = 0;
  miniz_uint16 method = 0, dos_time = 0, dos_date = 0;
  miniz_uint level, num_alignment_padding_bytes;
  miniz_uint64 local_dir_header_ofs, cur_archive_file_ofs, comp_size = 0;
  size_t archive_name_size;
  miniz_uint8 local_dir_header[MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE];
  tdefl_compressor *pComp = NULL;
  miniz_bool store_data_uncompressed;
  miniz_zip_internal_state *pState;

  if ((int)level_and_flags < 0)
    level_and_flags = MINIZ_DEFAULT_LEVEL;
  level = level_and_flags & 0xF;
  store_data_uncompressed =
      ((!level) || (level_and_flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA));

  if ((!pZip) || (!pZip->m_pState) ||
      (pZip->m_zip_mode != MINIZ_ZIP_MODE_WRITING) || ((buf_size) && (!pBuf)) ||
      (!pArchive_name) || ((comment_size) && (!pComment)) ||
      (pZip->m_total_files == 0xFFFF) || (level > MINIZ_UBER_COMPRESSION))
    return MINIZ_FALSE;

  local_dir_header_ofs = cur_archive_file_ofs = pZip->m_archive_size;
  pState = pZip->m_pState;

  if ((!(level_and_flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA)) && (uncomp_size))
    return MINIZ_FALSE;
  // No zip64 support yet
  if ((buf_size > 0xFFFFFFFF) || (uncomp_size > 0xFFFFFFFF))
    return MINIZ_FALSE;
  if (!miniz_zip_writer_validate_archive_name(pArchive_name))
    return MINIZ_FALSE;

#ifndef MINIZ_NO_TIME
  {
    time_t cur_time;
    time(&cur_time);
    miniz_zip_time_t_to_dos_time(cur_time, &dos_time, &dos_date);
  }
#endif // #ifndef MINIZ_NO_TIME

  archive_name_size = strlen(pArchive_name);
  if (archive_name_size > 0xFFFF)
    return MINIZ_FALSE;

  num_alignment_padding_bytes =
      miniz_zip_writer_compute_padding_needed_for_file_alignment(pZip);

  // no zip64 support yet
  if ((pZip->m_total_files == 0xFFFF) ||
      ((pZip->m_archive_size + num_alignment_padding_bytes +
        MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
        comment_size + archive_name_size) > 0xFFFFFFFF))
    return MINIZ_FALSE;

  if ((archive_name_size) && (pArchive_name[archive_name_size - 1] == '/')) {
    // Set DOS Subdirectory attribute bit.
    ext_attributes |= 0x10;
    // Subdirectories cannot contain data.
    if ((buf_size) || (uncomp_size))
      return MINIZ_FALSE;
  }

  // Try to do any allocations before writing to the archive, so if an
  // allocation fails the file remains unmodified. (A good idea if we're doing
  // an in-place modification.)
  if ((!miniz_zip_array_ensure_room(pZip, &pState->m_central_dir,
                                 MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
                                     archive_name_size + comment_size)) ||
      (!miniz_zip_array_ensure_room(pZip, &pState->m_central_dir_offsets, 1)))
    return MINIZ_FALSE;

  if ((!store_data_uncompressed) && (buf_size)) {
    if (NULL == (pComp = (tdefl_compressor *)pZip->m_pAlloc(
                     pZip->m_pAlloc_opaque, 1, sizeof(tdefl_compressor))))
      return MINIZ_FALSE;
  }

  if (!miniz_zip_writer_write_zeros(pZip, cur_archive_file_ofs,
                                 num_alignment_padding_bytes +
                                     sizeof(local_dir_header))) {
    pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);
    return MINIZ_FALSE;
  }
  local_dir_header_ofs += num_alignment_padding_bytes;
  if (pZip->m_file_offset_alignment) {
    MINIZ_ASSERT((local_dir_header_ofs & (pZip->m_file_offset_alignment - 1)) ==
              0);
  }
  cur_archive_file_ofs +=
      num_alignment_padding_bytes + sizeof(local_dir_header);

  MINIZ_CLEAR_OBJ(local_dir_header);
  if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pArchive_name,
                     archive_name_size) != archive_name_size) {
    pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);
    return MINIZ_FALSE;
  }
  cur_archive_file_ofs += archive_name_size;

  if (!(level_and_flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA)) {
    uncomp_crc32 =
        (miniz_uint32)miniz_crc32(MINIZ_CRC32_INIT, (const miniz_uint8 *)pBuf, buf_size);
    uncomp_size = buf_size;
    if (uncomp_size <= 3) {
      level = 0;
      store_data_uncompressed = MINIZ_TRUE;
    }
  }

  if (store_data_uncompressed) {
    if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pBuf,
                       buf_size) != buf_size) {
      pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);
      return MINIZ_FALSE;
    }

    cur_archive_file_ofs += buf_size;
    comp_size = buf_size;

    if (level_and_flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA)
      method = MINIZ_DEFLATED;
  } else if (buf_size) {
    miniz_zip_writer_add_state state;

    state.m_pZip = pZip;
    state.m_cur_archive_file_ofs = cur_archive_file_ofs;
    state.m_comp_size = 0;

    if ((tdefl_init(pComp, miniz_zip_writer_add_put_buf_callback, &state,
                    tdefl_create_comp_flags_from_zip_params(
                        level, -15, MINIZ_DEFAULT_STRATEGY)) !=
         TDEFL_STATUS_OKAY) ||
        (tdefl_compress_buffer(pComp, pBuf, buf_size, TDEFL_FINISH) !=
         TDEFL_STATUS_DONE)) {
      pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);
      return MINIZ_FALSE;
    }

    comp_size = state.m_comp_size;
    cur_archive_file_ofs = state.m_cur_archive_file_ofs;

    method = MINIZ_DEFLATED;
  }

  pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);
  pComp = NULL;

  // no zip64 support yet
  if ((comp_size > 0xFFFFFFFF) || (cur_archive_file_ofs > 0xFFFFFFFF))
    return MINIZ_FALSE;

  if (!miniz_zip_writer_create_local_dir_header(
          pZip, local_dir_header, (miniz_uint16)archive_name_size, 0, uncomp_size,
          comp_size, uncomp_crc32, method, 0, dos_time, dos_date))
    return MINIZ_FALSE;

  if (pZip->m_pWrite(pZip->m_pIO_opaque, local_dir_header_ofs, local_dir_header,
                     sizeof(local_dir_header)) != sizeof(local_dir_header))
    return MINIZ_FALSE;

  if (!miniz_zip_writer_add_to_central_dir(
          pZip, pArchive_name, (miniz_uint16)archive_name_size, NULL, 0, pComment,
          comment_size, uncomp_size, comp_size, uncomp_crc32, method, 0,
          dos_time, dos_date, local_dir_header_ofs, ext_attributes))
    return MINIZ_FALSE;

  pZip->m_total_files++;
  pZip->m_archive_size = cur_archive_file_ofs;

  return MINIZ_TRUE;
}

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_writer_add_file(miniz_zip_archive *pZip, const char *pArchive_name,
                               const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword, const void *pComment,
                               miniz_uint16 comment_size, miniz_uint level_and_flags,
                               miniz_uint32 ext_attributes) {
  miniz_uint uncomp_crc32 = MINIZ_CRC32_INIT, level, num_alignment_padding_bytes;
  miniz_uint16 method = 0, dos_time = 0, dos_date = 0;
  time_t file_modified_time;
  miniz_uint64 local_dir_header_ofs, cur_archive_file_ofs, uncomp_size = 0,
                                                        comp_size = 0;
  size_t archive_name_size;
  miniz_uint8 local_dir_header[MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE];
  MINIZ_FILE pSrc_file;

  if ((int)level_and_flags < 0)
    level_and_flags = MINIZ_DEFAULT_LEVEL;
  level = level_and_flags & 0xF;

  if ((!pZip) || (!pZip->m_pState) ||
      (pZip->m_zip_mode != MINIZ_ZIP_MODE_WRITING) || (!pArchive_name) ||
      ((comment_size) && (!pComment)) || (level > MINIZ_UBER_COMPRESSION))
    return MINIZ_FALSE;

  local_dir_header_ofs = cur_archive_file_ofs = pZip->m_archive_size;

  if (level_and_flags & MINIZ_ZIP_FLAG_COMPRESSED_DATA)
    return MINIZ_FALSE;
  if (!miniz_zip_writer_validate_archive_name(pArchive_name))
    return MINIZ_FALSE;

  archive_name_size = strlen(pArchive_name);
  if (archive_name_size > 0xFFFF)
    return MINIZ_FALSE;

  num_alignment_padding_bytes =
      miniz_zip_writer_compute_padding_needed_for_file_alignment(pZip);

  // no zip64 support yet
  if ((pZip->m_total_files == 0xFFFF) ||
      ((pZip->m_archive_size + num_alignment_padding_bytes +
        MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
        comment_size + archive_name_size) > 0xFFFFFFFF))
    return MINIZ_FALSE;

  memset(&file_modified_time, 0, sizeof(file_modified_time));
  if (!miniz_zip_get_file_modified_time(resourceDirectory, fileName, &file_modified_time))
    return MINIZ_FALSE;
  miniz_zip_time_t_to_dos_time(file_modified_time, &dos_time, &dos_date);

  if (!MINIZ_FOPEN(resourceDirectory, fileName, filePassword, "rb", &pSrc_file))
    return MINIZ_FALSE;
  MINIZ_FSEEK64(&pSrc_file, 0, SEEK_END);
  uncomp_size = MINIZ_FTELL64(&pSrc_file);
  MINIZ_FSEEK64(&pSrc_file, 0, SEEK_SET);

  if (uncomp_size > 0xFFFFFFFF) {
    // No zip64 support yet
    MINIZ_FCLOSE(&pSrc_file);
    return MINIZ_FALSE;
  }
  if (uncomp_size <= 3)
    level = 0;

  if (!miniz_zip_writer_write_zeros(pZip, cur_archive_file_ofs,
                                 num_alignment_padding_bytes +
                                     sizeof(local_dir_header))) {
    MINIZ_FCLOSE(&pSrc_file);
    return MINIZ_FALSE;
  }
  local_dir_header_ofs += num_alignment_padding_bytes;
  if (pZip->m_file_offset_alignment) {
    MINIZ_ASSERT((local_dir_header_ofs & (pZip->m_file_offset_alignment - 1)) ==
              0);
  }
  cur_archive_file_ofs +=
      num_alignment_padding_bytes + sizeof(local_dir_header);

  MINIZ_CLEAR_OBJ(local_dir_header);
  if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pArchive_name,
                     archive_name_size) != archive_name_size) {
    MINIZ_FCLOSE(&pSrc_file);
    return MINIZ_FALSE;
  }
  cur_archive_file_ofs += archive_name_size;

  if (uncomp_size) {
    miniz_uint64 uncomp_remaining = uncomp_size;
    void *pRead_buf =
        pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, MINIZ_ZIP_MAX_IO_BUF_SIZE);
    if (!pRead_buf) {
      MINIZ_FCLOSE(&pSrc_file);
      return MINIZ_FALSE;
    }

    if (!level) {
      while (uncomp_remaining) {
        miniz_uint n = (miniz_uint)MINIZ_MIN(MINIZ_ZIP_MAX_IO_BUF_SIZE, uncomp_remaining);
        if ((MINIZ_FREAD(pRead_buf, 1, n, &pSrc_file) != n) ||
            (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pRead_buf,
                            n) != n)) {
          pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
          MINIZ_FCLOSE(&pSrc_file);
          return MINIZ_FALSE;
        }
        uncomp_crc32 =
            (miniz_uint32)miniz_crc32(uncomp_crc32, (const miniz_uint8 *)pRead_buf, n);
        uncomp_remaining -= n;
        cur_archive_file_ofs += n;
      }
      comp_size = uncomp_size;
    } else {
      miniz_bool result = MINIZ_FALSE;
      miniz_zip_writer_add_state state;
      tdefl_compressor *pComp = (tdefl_compressor *)pZip->m_pAlloc(
          pZip->m_pAlloc_opaque, 1, sizeof(tdefl_compressor));
      if (!pComp) {
        pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
        MINIZ_FCLOSE(&pSrc_file);
        return MINIZ_FALSE;
      }

      state.m_pZip = pZip;
      state.m_cur_archive_file_ofs = cur_archive_file_ofs;
      state.m_comp_size = 0;

      if (tdefl_init(pComp, miniz_zip_writer_add_put_buf_callback, &state,
                     tdefl_create_comp_flags_from_zip_params(
                         level, -15, MINIZ_DEFAULT_STRATEGY)) !=
          TDEFL_STATUS_OKAY) {
        pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);
        pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
        MINIZ_FCLOSE(&pSrc_file);
        return MINIZ_FALSE;
      }

      for (;;) {
        size_t in_buf_size =
            (miniz_uint32)MINIZ_MIN(uncomp_remaining, MINIZ_ZIP_MAX_IO_BUF_SIZE);
        tdefl_status status;

        if (MINIZ_FREAD(pRead_buf, 1, in_buf_size, &pSrc_file) != in_buf_size)
          break;

        uncomp_crc32 = (miniz_uint32)miniz_crc32(
            uncomp_crc32, (const miniz_uint8 *)pRead_buf, in_buf_size);
        uncomp_remaining -= in_buf_size;

        status = tdefl_compress_buffer(pComp, pRead_buf, in_buf_size,
                                       uncomp_remaining ? TDEFL_NO_FLUSH
                                                        : TDEFL_FINISH);
        if (status == TDEFL_STATUS_DONE) {
          result = MINIZ_TRUE;
          break;
        } else if (status != TDEFL_STATUS_OKAY)
          break;
      }

      pZip->m_pFree(pZip->m_pAlloc_opaque, pComp);

      if (!result) {
        pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
        MINIZ_FCLOSE(&pSrc_file);
        return MINIZ_FALSE;
      }

      comp_size = state.m_comp_size;
      cur_archive_file_ofs = state.m_cur_archive_file_ofs;

      method = MINIZ_DEFLATED;
    }

    pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
  }

  MINIZ_FCLOSE(&pSrc_file);
  pSrc_file = (MINIZ_FILE){0};

  // no zip64 support yet
  if ((comp_size > 0xFFFFFFFF) || (cur_archive_file_ofs > 0xFFFFFFFF))
    return MINIZ_FALSE;

  if (!miniz_zip_writer_create_local_dir_header(
          pZip, local_dir_header, (miniz_uint16)archive_name_size, 0, uncomp_size,
          comp_size, uncomp_crc32, method, 0, dos_time, dos_date))
    return MINIZ_FALSE;

  if (pZip->m_pWrite(pZip->m_pIO_opaque, local_dir_header_ofs, local_dir_header,
                     sizeof(local_dir_header)) != sizeof(local_dir_header))
    return MINIZ_FALSE;

  if (!miniz_zip_writer_add_to_central_dir(
          pZip, pArchive_name, (miniz_uint16)archive_name_size, NULL, 0, pComment,
          comment_size, uncomp_size, comp_size, uncomp_crc32, method, 0,
          dos_time, dos_date, local_dir_header_ofs, ext_attributes))
    return MINIZ_FALSE;

  pZip->m_total_files++;
  pZip->m_archive_size = cur_archive_file_ofs;

  return MINIZ_TRUE;
}
#endif // #ifndef MINIZ_NO_STDIO

miniz_bool miniz_zip_writer_add_from_zip_reader(miniz_zip_archive *pZip,
                                          miniz_zip_archive *pSource_zip,
                                          miniz_uint file_index) {
  miniz_uint n, bit_flags, num_alignment_padding_bytes;
  miniz_uint64 comp_bytes_remaining, local_dir_header_ofs;
  miniz_uint64 cur_src_file_ofs, cur_dst_file_ofs;
  miniz_uint32
      local_header_u32[(MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(miniz_uint32) - 1) /
                       sizeof(miniz_uint32)];
  miniz_uint8 *pLocal_header = (miniz_uint8 *)local_header_u32;
  miniz_uint8 central_header[MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE];
  size_t orig_central_dir_size;
  miniz_zip_internal_state *pState;
  void *pBuf;
  const miniz_uint8 *pSrc_central_header;

  if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MINIZ_ZIP_MODE_WRITING))
    return MINIZ_FALSE;
  if (NULL ==
      (pSrc_central_header = miniz_zip_reader_get_cdh(pSource_zip, file_index)))
    return MINIZ_FALSE;
  pState = pZip->m_pState;

  num_alignment_padding_bytes =
      miniz_zip_writer_compute_padding_needed_for_file_alignment(pZip);

  // no zip64 support yet
  if ((pZip->m_total_files == 0xFFFF) ||
      ((pZip->m_archive_size + num_alignment_padding_bytes +
        MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE) >
       0xFFFFFFFF))
    return MINIZ_FALSE;

  cur_src_file_ofs =
      MINIZ_READ_LE32(pSrc_central_header + MINIZ_ZIP_CDH_LOCAL_HEADER_OFS);
  cur_dst_file_ofs = pZip->m_archive_size;

  if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs,
                           pLocal_header, MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE) !=
      MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE)
    return MINIZ_FALSE;
  if (MINIZ_READ_LE32(pLocal_header) != MINIZ_ZIP_LOCAL_DIR_HEADER_SIG)
    return MINIZ_FALSE;
  cur_src_file_ofs += MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE;

  if (!miniz_zip_writer_write_zeros(pZip, cur_dst_file_ofs,
                                 num_alignment_padding_bytes))
    return MINIZ_FALSE;
  cur_dst_file_ofs += num_alignment_padding_bytes;
  local_dir_header_ofs = cur_dst_file_ofs;
  if (pZip->m_file_offset_alignment) {
    MINIZ_ASSERT((local_dir_header_ofs & (pZip->m_file_offset_alignment - 1)) ==
              0);
  }

  if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_dst_file_ofs, pLocal_header,
                     MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE) !=
      MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE)
    return MINIZ_FALSE;
  cur_dst_file_ofs += MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE;

  n = MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_FILENAME_LEN_OFS) +
      MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_EXTRA_LEN_OFS);
  comp_bytes_remaining =
      n + MINIZ_READ_LE32(pSrc_central_header + MINIZ_ZIP_CDH_COMPRESSED_SIZE_OFS);

  if (NULL ==
      (pBuf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1,
                             (size_t)MINIZ_MAX(sizeof(miniz_uint32) * 4,
                                            MINIZ_MIN(MINIZ_ZIP_MAX_IO_BUF_SIZE,
                                                   comp_bytes_remaining)))))
    return MINIZ_FALSE;

  while (comp_bytes_remaining) {
    n = (miniz_uint)MINIZ_MIN(MINIZ_ZIP_MAX_IO_BUF_SIZE, comp_bytes_remaining);
    if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs, pBuf,
                             n) != n) {
      pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
      return MINIZ_FALSE;
    }
    cur_src_file_ofs += n;

    if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_dst_file_ofs, pBuf, n) != n) {
      pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
      return MINIZ_FALSE;
    }
    cur_dst_file_ofs += n;

    comp_bytes_remaining -= n;
  }

  bit_flags = MINIZ_READ_LE16(pLocal_header + MINIZ_ZIP_LDH_BIT_FLAG_OFS);
  if (bit_flags & 8) {
    // Copy data descriptor
    if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs, pBuf,
                             sizeof(miniz_uint32) * 4) != sizeof(miniz_uint32) * 4) {
      pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
      return MINIZ_FALSE;
    }

    n = sizeof(miniz_uint32) * ((MINIZ_READ_LE32(pBuf) == 0x08074b50) ? 4 : 3);
    if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_dst_file_ofs, pBuf, n) != n) {
      pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
      return MINIZ_FALSE;
    }

    // cur_src_file_ofs += n;
    cur_dst_file_ofs += n;
  }
  pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);

  // no zip64 support yet
  if (cur_dst_file_ofs > 0xFFFFFFFF)
    return MINIZ_FALSE;

  orig_central_dir_size = pState->m_central_dir.m_size;

  memcpy(central_header, pSrc_central_header, MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE);
  MINIZ_WRITE_LE32(central_header + MINIZ_ZIP_CDH_LOCAL_HEADER_OFS,
                local_dir_header_ofs);
  if (!miniz_zip_array_push_back(pZip, &pState->m_central_dir, central_header,
                              MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE))
    return MINIZ_FALSE;

  n = MINIZ_READ_LE16(pSrc_central_header + MINIZ_ZIP_CDH_FILENAME_LEN_OFS) +
      MINIZ_READ_LE16(pSrc_central_header + MINIZ_ZIP_CDH_EXTRA_LEN_OFS) +
      MINIZ_READ_LE16(pSrc_central_header + MINIZ_ZIP_CDH_COMMENT_LEN_OFS);
  if (!miniz_zip_array_push_back(
          pZip, &pState->m_central_dir,
          pSrc_central_header + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE, n)) {
    miniz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size,
                        MINIZ_FALSE);
    return MINIZ_FALSE;
  }

  if (pState->m_central_dir.m_size > 0xFFFFFFFF)
    return MINIZ_FALSE;
  n = (miniz_uint32)orig_central_dir_size;
  if (!miniz_zip_array_push_back(pZip, &pState->m_central_dir_offsets, &n, 1)) {
    miniz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size,
                        MINIZ_FALSE);
    return MINIZ_FALSE;
  }

  pZip->m_total_files++;
  pZip->m_archive_size = cur_dst_file_ofs;

  return MINIZ_TRUE;
}

miniz_bool miniz_zip_writer_finalize_archive(miniz_zip_archive *pZip) {
  miniz_zip_internal_state *pState;
  miniz_uint64 central_dir_ofs, central_dir_size;
  miniz_uint8 hdr[MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE];

  if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MINIZ_ZIP_MODE_WRITING))
    return MINIZ_FALSE;

  pState = pZip->m_pState;

  // no zip64 support yet
  if ((pZip->m_total_files > 0xFFFF) ||
      ((pZip->m_archive_size + pState->m_central_dir.m_size +
        MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE) > 0xFFFFFFFF))
    return MINIZ_FALSE;

  central_dir_ofs = 0;
  central_dir_size = 0;
  if (pZip->m_total_files) {
    // Write central directory
    central_dir_ofs = pZip->m_archive_size;
    central_dir_size = pState->m_central_dir.m_size;
    pZip->m_central_directory_file_ofs = central_dir_ofs;
    if (pZip->m_pWrite(pZip->m_pIO_opaque, central_dir_ofs,
                       pState->m_central_dir.m_p,
                       (size_t)central_dir_size) != central_dir_size)
      return MINIZ_FALSE;
    pZip->m_archive_size += central_dir_size;
  }

  // Write end of central directory record
  MINIZ_CLEAR_OBJ(hdr);
  MINIZ_WRITE_LE32(hdr + MINIZ_ZIP_ECDH_SIG_OFS,
                MINIZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG);
  MINIZ_WRITE_LE16(hdr + MINIZ_ZIP_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS,
                pZip->m_total_files);
  MINIZ_WRITE_LE16(hdr + MINIZ_ZIP_ECDH_CDIR_TOTAL_ENTRIES_OFS, pZip->m_total_files);
  MINIZ_WRITE_LE32(hdr + MINIZ_ZIP_ECDH_CDIR_SIZE_OFS, central_dir_size);
  MINIZ_WRITE_LE32(hdr + MINIZ_ZIP_ECDH_CDIR_OFS_OFS, central_dir_ofs);

  if (pZip->m_pWrite(pZip->m_pIO_opaque, pZip->m_archive_size, hdr,
                     sizeof(hdr)) != sizeof(hdr))
    return MINIZ_FALSE;
#ifndef MINIZ_NO_STDIO
  if ((pState->m_pFile.pFile) && (MINIZ_FFLUSH(&pState->m_pFile) == EOF))
    return MINIZ_FALSE;
#endif // #ifndef MINIZ_NO_STDIO

  pZip->m_archive_size += sizeof(hdr);

  pZip->m_zip_mode = MINIZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED;
  return MINIZ_TRUE;
}

miniz_bool miniz_zip_writer_finalize_heap_archive(miniz_zip_archive *pZip, void **pBuf,
                                            size_t *pSize) {
  if ((!pZip) || (!pZip->m_pState) || (!pBuf) || (!pSize))
    return MINIZ_FALSE;
  if (pZip->m_pWrite != miniz_zip_heap_write_func)
    return MINIZ_FALSE;
  if (!miniz_zip_writer_finalize_archive(pZip))
    return MINIZ_FALSE;

  *pBuf = pZip->m_pState->m_pMem;
  *pSize = pZip->m_pState->m_mem_size;
  pZip->m_pState->m_pMem = NULL;
  pZip->m_pState->m_mem_size = pZip->m_pState->m_mem_capacity = 0;
  return MINIZ_TRUE;
}

miniz_bool miniz_zip_writer_end(miniz_zip_archive *pZip) {
  miniz_zip_internal_state *pState;
  miniz_bool status = MINIZ_TRUE;
  if ((!pZip) || (!pZip->m_pState) || (!pZip->m_pAlloc) || (!pZip->m_pFree) ||
      ((pZip->m_zip_mode != MINIZ_ZIP_MODE_WRITING) &&
       (pZip->m_zip_mode != MINIZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED)))
    return MINIZ_FALSE;

  pState = pZip->m_pState;
  pZip->m_pState = NULL;
  miniz_zip_array_clear(pZip, &pState->m_central_dir);
  miniz_zip_array_clear(pZip, &pState->m_central_dir_offsets);
  miniz_zip_array_clear(pZip, &pState->m_sorted_central_dir_offsets);

#ifndef MINIZ_NO_STDIO
  if (pState->m_pFile.pFile) {
    MINIZ_FCLOSE(&pState->m_pFile);
  }
#endif // #ifndef MINIZ_NO_STDIO

  if ((pZip->m_pWrite == miniz_zip_heap_write_func) && (pState->m_pMem)) {
    pZip->m_pFree(pZip->m_pAlloc_opaque, pState->m_pMem);
    pState->m_pMem = NULL;
  }

  pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
  pZip->m_zip_mode = MINIZ_ZIP_MODE_INVALID;
  return status;
}

#ifndef MINIZ_NO_STDIO
// CONFFX_CHANGE - Custom File IO
miniz_bool miniz_zip_add_mem_to_archive_file_in_place(
    const ResourceDirectory resourceDirectory, const char* fileName, const char* filePassword, const char *pArchive_name, const void *pBuf,
    size_t buf_size, const void *pComment, miniz_uint16 comment_size,
    miniz_uint level_and_flags) {
  miniz_bool status, created_new_archive = MINIZ_FALSE;
  miniz_zip_archive zip_archive;
  MINIZ_CLEAR_OBJ(zip_archive);
  if ((int)level_and_flags < 0)
    level_and_flags = MINIZ_DEFAULT_LEVEL;
  if ((!pArchive_name) || ((buf_size) && (!pBuf)) ||
      ((comment_size) && (!pComment)) ||
      ((level_and_flags & 0xF) > MINIZ_UBER_COMPRESSION))
    return MINIZ_FALSE;
  if (!miniz_zip_writer_validate_archive_name(pArchive_name))
    return MINIZ_FALSE;
 // if (!fsFileExists(resourceDirectory, fileName)) {
 //   // Create a new archive.
 //   if (!miniz_zip_writer_init_file(&zip_archive, resourceDirectory, fileName, 0))
 //     return MINIZ_FALSE;
 //   created_new_archive = MINIZ_TRUE;
 // } else {
    // Append to an existing archive.
    if (!miniz_zip_reader_init_file(&zip_archive, resourceDirectory, fileName, filePassword,
                                 level_and_flags | MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY))
      return MINIZ_FALSE;
    if (!miniz_zip_writer_init_from_reader(&zip_archive, resourceDirectory, fileName)) {
      miniz_zip_reader_end(&zip_archive);
      return MINIZ_FALSE;
    }
  //}
  status =
      miniz_zip_writer_add_mem_ex(&zip_archive, pArchive_name, pBuf, buf_size,
                               pComment, comment_size, level_and_flags, 0, 0);
  // Always finalize, even if adding failed for some reason, so we have a valid
  // central directory. (This may not always succeed, but we can try.)
  if (!miniz_zip_writer_finalize_archive(&zip_archive))
    status = MINIZ_FALSE;
  if (!miniz_zip_writer_end(&zip_archive))
    status = MINIZ_FALSE;
  if ((!status) && (created_new_archive)) {
    // It's a new archive and something went wrong, so just delete it.
    int ignoredStatus = MINIZ_DELETE_FILE(resourceDirectory, fileName);
    (void)ignoredStatus;
  }
  return status;
}

// CONFFX_CHANGE - Custom File IO
void *miniz_zip_extract_archive_file_to_heap(const ResourceDirectory resourceDirectory, const char* fileName, 
	                                      const char* filePassword,
                                          const char *pArchive_name,
                                          size_t *pSize, miniz_uint flags) {
  int file_index;
  miniz_zip_archive zip_archive;
  void *p = NULL;

  if (pSize)
    *pSize = 0;

  if (!pArchive_name)
    return NULL;

  MINIZ_CLEAR_OBJ(zip_archive);
  if (!miniz_zip_reader_init_file(&zip_archive, resourceDirectory, fileName, filePassword,
                               flags |
                                   MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY))
    return NULL;

  if ((file_index = miniz_zip_reader_locate_file(&zip_archive, pArchive_name, NULL,
                                              flags)) >= 0)
    p = miniz_zip_reader_extract_to_heap(&zip_archive, file_index, pSize, flags);

  miniz_zip_reader_end(&zip_archive);
  return p;
}

#endif // #ifndef MINIZ_NO_STDIO

#endif // #ifndef MINIZ_NO_ARCHIVE_WRITING_APIS

#endif // #ifndef MINIZ_NO_ARCHIVE_APIS

#ifdef __cplusplus
}
#endif

#endif // MINIZ_HEADER_FILE_ONLY

/*
  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/
