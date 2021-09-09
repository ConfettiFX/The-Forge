/*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#define __STDC_WANT_LIB_EXT1__ 1

#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER) ||              \
    defined(__MINGW32__)
/* Win32, DOS, MSVC, MSVS */
#include <direct.h>

#define MKDIR(DIRNAME) _mkdir(DIRNAME)
#define STRCLONE(STR) ((STR) ? _strdup(STR) : NULL)
#define HAS_DEVICE(P)                                                          \
  ((((P)[0] >= 'A' && (P)[0] <= 'Z') || ((P)[0] >= 'a' && (P)[0] <= 'z')) &&   \
   (P)[1] == ':')
#define FILESYSTEM_PREFIX_LEN(P) (HAS_DEVICE(P) ? 2 : 0)

#else

#include <unistd.h> // needed for symlink() on BSD

#if defined(ORBIS) || defined(PROSPERO)
// Not supported
int symlink(const char *target, const char *linkpath) { return -1; }
#else
int symlink(const char *target, const char *linkpath); // needed on Linux
#endif

#define MKDIR(DIRNAME) mkdir(DIRNAME, 0755)
#define STRCLONE(STR) ((STR) ? strdup(STR) : NULL)

#endif

#include "miniz.h"
#include "zip.h"

// CONFFX_BEGIN - Custom Allocator
#include "../../../OS/Interfaces/IMemory.h"
// CONFFX_END

//#ifndef MAX_PATH
//#define MAX_PATH 32767 /* # chars in a path name including NULL */
//#endif

#ifndef HAS_DEVICE
#define HAS_DEVICE(P) 0
#endif

#ifndef FILESYSTEM_PREFIX_LEN
#define FILESYSTEM_PREFIX_LEN(P) 0
#endif

#ifndef ISSLASH
#define ISSLASH(C) ((C) == '/' || (C) == '\\')
#endif

// CONFFX_BEGIN - Custom Allocator
#define CLEANUP(ptr)                                                           \
  do {                                                                         \
    if (ptr) {                                                                 \
      tf_free((void *)ptr);                                                  \
      ptr = NULL;                                                              \
    }                                                                          \
  } while (0)
// CONFFX_END

static char *strrpl(const char *str, size_t n, char oldchar, char newchar) {
  char c;
  size_t i;
  // CONFFX_BEGIN - Custom Allocator
  char *rpl = (char *)tf_calloc((1 + n), sizeof(char));
  // CONFFX_END
  char *begin = rpl;
  if (!rpl) {
    return NULL;
  }

  for (i = 0; (i < n) && (c = *str++); ++i) {
    if (c == oldchar) {
      c = newchar;
    }
    *rpl++ = c;
  }

  return begin;
}

struct zip_entry_t {
  int index;
  char *name;
  miniz_uint64 uncomp_size;
  miniz_uint64 comp_size;
  miniz_uint32 uncomp_crc32;
  miniz_uint64 offset;
  miniz_uint8 header[MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE];
  miniz_uint64 header_offset;
  miniz_uint16 method;
  miniz_zip_writer_add_state state;
  tdefl_compressor comp;
  miniz_uint32 external_attr;
  time_t m_time;
};

struct zip_t {
  miniz_zip_archive archive;
  miniz_uint level;
  struct zip_entry_t entry;
};

// CONFFX_BEGIN - Custom Allocator
// Heap allocation callbacks.
// Note that miniz_alloc_func parameter types purpsosely differ from zlib's:
// items/size is size_t, not unsigned long.
static void* tf_miniz_alloc_func(void *opaque, size_t items, size_t size)
{
	return tf_calloc(items, size);
}

static void tf_miniz_free_func(void *opaque, void *address)
{
	tf_free(address);
}

static void* tf_miniz_realloc_func(void *opaque, void *address, size_t items, size_t size)
{
	return tf_realloc(address, items * size);
}
// CONFFX_END

// CONFFX_CHANGE - Custom File IO
struct zip_t *zip_open(ResourceDirectory resourceDirectory, const char* fileName, int level, char mode) {
  struct zip_t *zip = NULL;

  if (!strlen(fileName)) {
    // zip_t archive name is empty or NULL
    goto cleanup;
  }

  if (level < 0)
    level = MINIZ_DEFAULT_LEVEL;
  if ((level & 0xF) > MINIZ_UBER_COMPRESSION) {
    // Wrong compression level
    goto cleanup;
  }

  // CONFFX_BEGIN - Custom Allocator
  zip = (struct zip_t *)tf_calloc((size_t)1, sizeof(struct zip_t));
  // CONFFX_END
  if (!zip)
    goto cleanup;

  zip->level = (miniz_uint)level;
  // CONFFX_BEGIN - Custom Allocator
  zip->archive.m_pAlloc = tf_miniz_alloc_func;
  zip->archive.m_pFree = tf_miniz_free_func;
  zip->archive.m_pRealloc = tf_miniz_realloc_func;
  // CONFFX_END

  switch (mode) {
  case 'w':
    // Create a new archive.
    if (!miniz_zip_writer_init_file(&(zip->archive), resourceDirectory, fileName, 0)) {
      // Cannot initialize zip_archive writer
      goto cleanup;
    }
    break;

  case 'r':
  case 'a':
    if (!miniz_zip_reader_init_file(
            &(zip->archive), resourceDirectory, fileName,
            zip->level | MINIZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY)) {
      // An archive file does not exist or cannot initialize
      // zip_archive reader
      goto cleanup;
    }
    if (mode == 'a' &&
        !miniz_zip_writer_init_from_reader(&(zip->archive), resourceDirectory, fileName)) {
      miniz_zip_reader_end(&(zip->archive));
      goto cleanup;
    }
    break;

  default:
    goto cleanup;
  }

  return zip;

cleanup:
  CLEANUP(zip);
  return NULL;
}

void zip_close(struct zip_t *zip) {
  if (zip) {
    // Always finalize, even if adding failed for some reason, so we have a
    // valid central directory.
    miniz_zip_writer_finalize_archive(&(zip->archive));

    miniz_zip_writer_end(&(zip->archive));
    miniz_zip_reader_end(&(zip->archive));

    CLEANUP(zip);
  }
}

int zip_is64(struct zip_t *zip) {
  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  if (!zip->archive.m_pState) {
    // zip state is not initialized
    return -1;
  }

  return (int)zip->archive.m_pState->m_zip64;
}

int zip_entry_open(struct zip_t *zip, const char *entryname) {
  size_t entrylen = 0;
  miniz_zip_archive *pzip = NULL;
  miniz_uint num_alignment_padding_bytes, level;
  miniz_zip_archive_file_stat stats;

  if (!zip || !entryname) {
    return -1;
  }

  entrylen = strlen(entryname);
  if (entrylen < 1) {
    return -1;
  }

  /*
    .ZIP File Format Specification Version: 6.3.3

    4.4.17.1 The name of the file, with optional relative path.
    The path stored MUST not contain a drive or
    device letter, or a leading slash.  All slashes
    MUST be forward slashes '/' as opposed to
    backwards slashes '\' for compatibility with Amiga
    and UNIX file systems etc.  If input came from standard
    input, there is no file name field.
  */
  zip->entry.name = strrpl(entryname, entrylen, '\\', '/');
  if (!zip->entry.name) {
    // Cannot parse zip entry name
    return -1;
  }

  pzip = &(zip->archive);
  if (pzip->m_zip_mode == MINIZ_ZIP_MODE_READING) {
    zip->entry.index =
        miniz_zip_reader_locate_file(pzip, zip->entry.name, NULL, 0);
    if (zip->entry.index < 0) {
      goto cleanup;
    }

    if (!miniz_zip_reader_file_stat(pzip, (miniz_uint)zip->entry.index, &stats)) {
      goto cleanup;
    }

    zip->entry.comp_size = stats.m_comp_size;
    zip->entry.uncomp_size = stats.m_uncomp_size;
    zip->entry.uncomp_crc32 = stats.m_crc32;
    zip->entry.offset = stats.m_central_dir_ofs;
    zip->entry.header_offset = stats.m_local_header_ofs;
    zip->entry.method = stats.m_method;
    zip->entry.external_attr = stats.m_external_attr;
    zip->entry.m_time = stats.m_time;

    return 0;
  }

  zip->entry.index = (int)zip->archive.m_total_files;
  zip->entry.comp_size = 0;
  zip->entry.uncomp_size = 0;
  zip->entry.uncomp_crc32 = MINIZ_CRC32_INIT;
  zip->entry.offset = zip->archive.m_archive_size;
  zip->entry.header_offset = zip->archive.m_archive_size;
  memset(zip->entry.header, 0, MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE * sizeof(miniz_uint8));
  zip->entry.method = 0;

  // UNIX or APPLE
#if MINIZ_PLATFORM == 3 || MINIZ_PLATFORM == 19
  // regular file with rw-r--r-- persmissions
  zip->entry.external_attr = (miniz_uint32)(0100644) << 16;
#else
  zip->entry.external_attr = 0;
#endif

  num_alignment_padding_bytes =
      miniz_zip_writer_compute_padding_needed_for_file_alignment(pzip);

  if (!pzip->m_pState || (pzip->m_zip_mode != MINIZ_ZIP_MODE_WRITING)) {
    // Wrong zip mode
    goto cleanup;
  }
  if (zip->level & MINIZ_ZIP_FLAG_COMPRESSED_DATA) {
    // Wrong zip compression level
    goto cleanup;
  }
  // no zip64 support yet
  if ((pzip->m_total_files == 0xFFFF) ||
      ((pzip->m_archive_size + num_alignment_padding_bytes +
        MINIZ_ZIP_LOCAL_DIR_HEADER_SIZE + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE +
        entrylen) > 0xFFFFFFFF)) {
    // No zip64 support yet
    goto cleanup;
  }
  if (!miniz_zip_writer_write_zeros(pzip, zip->entry.offset,
                                 num_alignment_padding_bytes +
                                     sizeof(zip->entry.header))) {
    // Cannot memset zip entry header
    goto cleanup;
  }

  zip->entry.header_offset += num_alignment_padding_bytes;
  if (pzip->m_file_offset_alignment) {
    MINIZ_ASSERT(
        (zip->entry.header_offset & (pzip->m_file_offset_alignment - 1)) == 0);
  }
  zip->entry.offset += num_alignment_padding_bytes + sizeof(zip->entry.header);

  if (pzip->m_pWrite(pzip->m_pIO_opaque, zip->entry.offset, zip->entry.name,
                     entrylen) != entrylen) {
    // Cannot write data to zip entry
    goto cleanup;
  }

  zip->entry.offset += entrylen;
  level = zip->level & 0xF;
  if (level) {
    zip->entry.state.m_pZip = pzip;
    zip->entry.state.m_cur_archive_file_ofs = zip->entry.offset;
    zip->entry.state.m_comp_size = 0;

    if (tdefl_init(&(zip->entry.comp), miniz_zip_writer_add_put_buf_callback,
                   &(zip->entry.state),
                   (int)tdefl_create_comp_flags_from_zip_params(
                       (int)level, -15, MINIZ_DEFAULT_STRATEGY)) !=
        TDEFL_STATUS_OKAY) {
      // Cannot initialize the zip compressor
      goto cleanup;
    }
  }

  zip->entry.m_time = time(NULL);

  return 0;

cleanup:
  CLEANUP(zip->entry.name);
  return -1;
}

int zip_entry_openbyindex(struct zip_t *zip, int index) {
  miniz_zip_archive *pZip = NULL;
  miniz_zip_archive_file_stat stats;
  miniz_uint namelen;
  const miniz_uint8 *pHeader;
  const char *pFilename;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  pZip = &(zip->archive);
  if (pZip->m_zip_mode != MINIZ_ZIP_MODE_READING) {
    // open by index requires readonly mode
    return -1;
  }

  if (index < 0 || (miniz_uint)index >= pZip->m_total_files) {
    // index out of range
    return -1;
  }

  if (!(pHeader = &MINIZ_ZIP_ARRAY_ELEMENT(
            &pZip->m_pState->m_central_dir, miniz_uint8,
            MINIZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets,
                                 miniz_uint32, index)))) {
    // cannot find header in central directory
    return -1;
  }

  namelen = MINIZ_READ_LE16(pHeader + MINIZ_ZIP_CDH_FILENAME_LEN_OFS);
  pFilename = (const char *)pHeader + MINIZ_ZIP_CENTRAL_DIR_HEADER_SIZE;

  /*
    .ZIP File Format Specification Version: 6.3.3

    4.4.17.1 The name of the file, with optional relative path.
    The path stored MUST not contain a drive or
    device letter, or a leading slash.  All slashes
    MUST be forward slashes '/' as opposed to
    backwards slashes '\' for compatibility with Amiga
    and UNIX file systems etc.  If input came from standard
    input, there is no file name field.
  */
  zip->entry.name = strrpl(pFilename, namelen, '\\', '/');
  if (!zip->entry.name) {
    // local entry name is NULL
    return -1;
  }

  if (!miniz_zip_reader_file_stat(pZip, (miniz_uint)index, &stats)) {
    return -1;
  }

  zip->entry.index = index;
  zip->entry.comp_size = stats.m_comp_size;
  zip->entry.uncomp_size = stats.m_uncomp_size;
  zip->entry.uncomp_crc32 = stats.m_crc32;
  zip->entry.offset = stats.m_central_dir_ofs;
  zip->entry.header_offset = stats.m_local_header_ofs;
  zip->entry.method = stats.m_method;
  zip->entry.external_attr = stats.m_external_attr;
  zip->entry.m_time = stats.m_time;

  return 0;
}

int zip_entry_close(struct zip_t *zip) {
  miniz_zip_archive *pzip = NULL;
  miniz_uint level;
  tdefl_status done;
  miniz_uint16 entrylen;
  miniz_uint16 dos_time, dos_date;
  int status = -1;

  if (!zip) {
    // zip_t handler is not initialized
    goto cleanup;
  }

  pzip = &(zip->archive);
  if (pzip->m_zip_mode == MINIZ_ZIP_MODE_READING) {
    status = 0;
    goto cleanup;
  }

  level = zip->level & 0xF;
  if (level) {
    done = tdefl_compress_buffer(&(zip->entry.comp), "", 0, TDEFL_FINISH);
    if (done != TDEFL_STATUS_DONE && done != TDEFL_STATUS_OKAY) {
      // Cannot flush compressed buffer
      goto cleanup;
    }
    zip->entry.comp_size = zip->entry.state.m_comp_size;
    zip->entry.offset = zip->entry.state.m_cur_archive_file_ofs;
    zip->entry.method = MINIZ_DEFLATED;
  }

  entrylen = (miniz_uint16)strlen(zip->entry.name);
  // no zip64 support yet
  if ((zip->entry.comp_size > 0xFFFFFFFF) || (zip->entry.offset > 0xFFFFFFFF)) {
    // No zip64 support, yet
    goto cleanup;
  }

  miniz_zip_time_t_to_dos_time(zip->entry.m_time, &dos_time, &dos_date);
  if (!miniz_zip_writer_create_local_dir_header(
          pzip, zip->entry.header, entrylen, 0, zip->entry.uncomp_size,
          zip->entry.comp_size, zip->entry.uncomp_crc32, zip->entry.method, 0,
          dos_time, dos_date)) {
    // Cannot create zip entry header
    goto cleanup;
  }

  if (pzip->m_pWrite(pzip->m_pIO_opaque, zip->entry.header_offset,
                     zip->entry.header,
                     sizeof(zip->entry.header)) != sizeof(zip->entry.header)) {
    // Cannot write zip entry header
    goto cleanup;
  }

  if (!miniz_zip_writer_add_to_central_dir(
          pzip, zip->entry.name, entrylen, NULL, 0, "", 0,
          zip->entry.uncomp_size, zip->entry.comp_size, zip->entry.uncomp_crc32,
          zip->entry.method, 0, dos_time, dos_date, zip->entry.header_offset,
          zip->entry.external_attr)) {
    // Cannot write to zip central dir
    goto cleanup;
  }

  pzip->m_total_files++;
  pzip->m_archive_size = zip->entry.offset;
  status = 0;

cleanup:
  if (zip) {
    zip->entry.m_time = 0;
    CLEANUP(zip->entry.name);
  }
  return status;
}

const char *zip_entry_name(struct zip_t *zip) {
  if (!zip) {
    // zip_t handler is not initialized
    return NULL;
  }

  return zip->entry.name;
}

int zip_entry_index(struct zip_t *zip) {
  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  return zip->entry.index;
}

int zip_entry_isdir(struct zip_t *zip) {
  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  if (zip->entry.index < 0) {
    // zip entry is not opened
    return -1;
  }

  return (int)miniz_zip_reader_is_file_a_directory(&zip->archive,
                                                (miniz_uint)zip->entry.index);
}

unsigned long long zip_entry_size(struct zip_t *zip) {
  return zip ? zip->entry.uncomp_size : 0;
}

unsigned int zip_entry_crc32(struct zip_t *zip) {
  return zip ? zip->entry.uncomp_crc32 : 0;
}

time_t zip_entry_time(struct zip_t *zip) {
	return zip ? zip->entry.m_time : 0;
}

int zip_entry_write(struct zip_t *zip, const void *buf, size_t bufsize) {
  miniz_uint level;
  miniz_zip_archive *pzip = NULL;
  tdefl_status status;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  pzip = &(zip->archive);
  if (buf && bufsize > 0) {
    zip->entry.uncomp_size += bufsize;
    zip->entry.uncomp_crc32 = (miniz_uint32)miniz_crc32(
        zip->entry.uncomp_crc32, (const miniz_uint8 *)buf, bufsize);

    level = zip->level & 0xF;
    if (!level) {
      if ((pzip->m_pWrite(pzip->m_pIO_opaque, zip->entry.offset, buf,
                          bufsize) != bufsize)) {
        // Cannot write buffer
        return -1;
      }
      zip->entry.offset += bufsize;
      zip->entry.comp_size += bufsize;
    } else {
      status = tdefl_compress_buffer(&(zip->entry.comp), buf, bufsize,
                                     TDEFL_NO_FLUSH);
      if (status != TDEFL_STATUS_DONE && status != TDEFL_STATUS_OKAY) {
        // Cannot compress buffer
        return -1;
      }
    }
  }

  return 0;
}

int zip_entry_fwrite(struct zip_t *zip, const char *filename) {
  int status = 0;
  size_t n = 0;
  FILE *stream = NULL;
  miniz_uint8 buf[MINIZ_ZIP_MAX_IO_BUF_SIZE];
  struct MINIZ_FILE_STAT_STRUCT file_stat;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  memset(buf, 0, MINIZ_ZIP_MAX_IO_BUF_SIZE);
  memset((void *)&file_stat, 0, sizeof(struct MINIZ_FILE_STAT_STRUCT));
  if (MINIZ_FILE_STAT(filename, &file_stat) != 0) {
    // problem getting information - check errno
    return -1;
  }

  if ((file_stat.st_mode & 0200) == 0) {
    // MS-DOS read-only attribute
    zip->entry.external_attr |= 0x01;
  }
  zip->entry.external_attr |= (miniz_uint32)((file_stat.st_mode & 0xFFFF) << 16);
  zip->entry.m_time = file_stat.st_mtime;

#if defined(_MSC_VER)
  if (fopen_s(&stream, filename, "rb"))
#else
  if (!(stream = fopen(filename, "rb")))
#endif
  {
    // Cannot open filename
    return -1;
  }

  while ((n = fread(buf, sizeof(miniz_uint8), MINIZ_ZIP_MAX_IO_BUF_SIZE, stream)) >
         0) {
    if (zip_entry_write(zip, buf, n) < 0) {
      status = -1;
      break;
    }
  }
  fclose(stream);

  return status;
}

ssize_t zip_entry_read(struct zip_t *zip, void **buf, size_t *bufsize) {
  miniz_zip_archive *pzip = NULL;
  miniz_uint idx;
  size_t size = 0;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  pzip = &(zip->archive);
  if (pzip->m_zip_mode != MINIZ_ZIP_MODE_READING || zip->entry.index < 0) {
    // the entry is not found or we do not have read access
    return -1;
  }

  idx = (miniz_uint)zip->entry.index;
  if (miniz_zip_reader_is_file_a_directory(pzip, idx)) {
    // the entry is a directory
    return -1;
  }

  *buf = miniz_zip_reader_extract_to_heap(pzip, idx, &size, 0);
  if (*buf && bufsize) {
    *bufsize = size;
  }
  return size;
}

ssize_t zip_entry_noallocread(struct zip_t *zip, void *buf, size_t bufsize) {
  miniz_zip_archive *pzip = NULL;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  pzip = &(zip->archive);
  if (pzip->m_zip_mode != MINIZ_ZIP_MODE_READING || zip->entry.index < 0) {
    // the entry is not found or we do not have read access
    return -1;
  }

  if (!miniz_zip_reader_extract_to_mem_no_alloc(pzip, (miniz_uint)zip->entry.index,
                                             buf, bufsize, 0, NULL, 0)) {
    return -1;
  }

  return (ssize_t)zip->entry.uncomp_size;
}

// CONFFX_CHANGE - Custom File IO
int zip_entry_fread(struct zip_t *zip, const ResourceDirectory resourceDirectory, const char* fileName) {
  miniz_zip_archive *pzip = NULL;
  miniz_uint idx;
  miniz_uint32 xattr = 0;
  miniz_zip_archive_file_stat info;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  memset((void *)&info, 0, sizeof(miniz_zip_archive_file_stat));
  pzip = &(zip->archive);
  if (pzip->m_zip_mode != MINIZ_ZIP_MODE_READING || zip->entry.index < 0) {
    // the entry is not found or we do not have read access
    return -1;
  }

  idx = (miniz_uint)zip->entry.index;
  if (miniz_zip_reader_is_file_a_directory(pzip, idx)) {
    // the entry is a directory
    return -1;
  }

  if (!miniz_zip_reader_extract_to_file(pzip, idx, resourceDirectory, fileName, 0)) {
    return -1;
  }

#if defined(_MSC_VER)
#else
  if (!miniz_zip_reader_file_stat(pzip, idx, &info)) {
    // Cannot get information about zip archive;
    return -1;
  }

  char path[FS_MAX_PATH] = {};
  fsAppendPathComponent(fsGetResourceDirectory(resourceDirectory), fileName, path);
  xattr = (info.m_external_attr >> 16) & 0xFFFF;
  if (xattr > 0) {
    if (chmod(path, (mode_t)xattr) < 0) {
      return -1;
    }
  }
#endif

  return 0;
}

int zip_entry_extract(struct zip_t *zip,
                      size_t (*on_extract)(void *arg, unsigned long long offset,
                                           const void *buf, size_t bufsize),
                      void *arg) {
  miniz_zip_archive *pzip = NULL;
  miniz_uint idx;

  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  pzip = &(zip->archive);
  if (pzip->m_zip_mode != MINIZ_ZIP_MODE_READING || zip->entry.index < 0) {
    // the entry is not found or we do not have read access
    return -1;
  }

  idx = (miniz_uint)zip->entry.index;
  return (miniz_zip_reader_extract_to_callback(pzip, idx, on_extract, arg, 0))
             ? 0
             : -1;
}

int zip_total_entries(struct zip_t *zip) {
  if (!zip) {
    // zip_t handler is not initialized
    return -1;
  }

  return (int)zip->archive.m_total_files;
}

// CONFFX_CHANGE - Custom File IO
int zip_create(const ResourceDirectory resourceDirectory, const char* fileName, const char *filenames[], size_t len) {
  int status = 0;
  size_t i;
  miniz_zip_archive zip_archive;
  struct MINIZ_FILE_STAT_STRUCT file_stat;
  miniz_uint32 ext_attributes = 0;

  if (!strlen(fileName)) {
    // zip_t archive name is empty or NULL
    return -1;
  }

  // Create a new archive.
  if (!memset(&(zip_archive), 0, sizeof(zip_archive))) {
    // Cannot memset zip archive
    return -1;
  }

  if (!miniz_zip_writer_init_file(&zip_archive, resourceDirectory, fileName, 0)) {
    // Cannot initialize zip_archive writer
    return -1;
  }

  memset((void *)&file_stat, 0, sizeof(struct MINIZ_FILE_STAT_STRUCT));

  for (i = 0; i < len; ++i) {
    const char *name = filenames[i];
    if (!name) {
      status = -1;
      break;
    }

	//if (!fsFileExists(resourceDirectory, name)) {
    //  // problem getting information - check errno
    //  status = -1;
    //  break;
    //}

    if ((file_stat.st_mode & 0200) == 0) {
      // MS-DOS read-only attribute
      ext_attributes |= 0x01;
    }
    ext_attributes |= (miniz_uint32)((file_stat.st_mode & 0xFFFF) << 16);
	char tempFileName[FS_MAX_PATH] = {};
	fsGetPathFileName(name, tempFileName);
    if (!miniz_zip_writer_add_file(&zip_archive, tempFileName, resourceDirectory, name, "", 0,
                                ZIP_DEFAULT_COMPRESSION_LEVEL,
                                ext_attributes)) {
      // Cannot add file to zip_archive
      status = -1;
      break;
    }
  }

  miniz_zip_writer_finalize_archive(&zip_archive);
  miniz_zip_writer_end(&zip_archive);
  return status;
}

// CONFFX_CHANGE - Custom File IO
int zip_extract(const ResourceDirectory resourceDirectory, const char* fileName, const ResourceDirectory dir,
                int (*on_extract)(const char *path, void *arg), void *arg) {
  int status = -1;
  miniz_uint i, n;
  char symlink_to[FS_MAX_PATH + 1];
  miniz_zip_archive zip_archive;
  miniz_zip_archive_file_stat info;
  miniz_uint32 xattr = 0;

  memset(symlink_to, 0, sizeof(symlink_to));
  if (!memset(&(zip_archive), 0, sizeof(zip_archive))) {
    // Cannot memset zip archive
    return -1;
  }

  if (!strlen(fileName) || !dir) {
    // Cannot parse zip archive name
    return -1;
  }

  // Now try to open the archive.
  if (!miniz_zip_reader_init_file(&zip_archive, resourceDirectory, fileName, 0)) {
    // Cannot initialize zip_archive reader
    return -1;
  }

  memset((void *)&info, 0, sizeof(miniz_zip_archive_file_stat));

  // Get and print information about each file in the archive.
  n = miniz_zip_reader_get_num_files(&zip_archive);
  for (i = 0; i < n; ++i) {
    if (!miniz_zip_reader_file_stat(&zip_archive, i, &info)) {
      // Cannot get information about zip archive;
      goto out;
    }

	char path[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(dir), info.m_filename, path);

    if ((((info.m_version_made_by >> 8) == 3) ||
         ((info.m_version_made_by >> 8) ==
          19)) // if zip is produced on Unix or macOS (3 and 19 from
               // section 4.4.2.2 of zip standard)
        && info.m_external_attr &
               (0x20 << 24)) { // and has sym link attribute (0x80 is file, 0x40
                               // is directory)
#if defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER) ||              \
    defined(__MINGW32__)
#else
      if (info.m_uncomp_size > FS_MAX_PATH ||
          !miniz_zip_reader_extract_to_mem_no_alloc(&zip_archive, i, symlink_to,
                                                 FS_MAX_PATH, 0, NULL, 0)) {
        goto out;
      }
      symlink_to[info.m_uncomp_size] = '\0';
      if (symlink(symlink_to, path) != 0) {
        goto out;
      }
#endif
    } else {
      if (!miniz_zip_reader_is_file_a_directory(&zip_archive, i)) {
        if (!miniz_zip_reader_extract_to_file(&zip_archive, i, resourceDirectory, path, 0)) {
          // Cannot extract zip archive to file
          goto out;
        }
      }

#if defined(_MSC_VER)
#else
      xattr = (info.m_external_attr >> 16) & 0xFFFF;
      if (xattr > 0) {
        if (chmod(path, (mode_t)xattr) < 0) {
          goto out;
        }
      }
#endif
    }

    if (on_extract) {
      if (on_extract(path, arg) < 0) {
        goto out;
      }
    }
  }
  status = 0;

out:
  // Close the archive, freeing any resources it was using
  if (!miniz_zip_reader_end(&zip_archive)) {
    // Cannot end zip reader
    status = -1;
  }

  return status;
}
