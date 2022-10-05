# MZ_ZIP <!-- omit in toc -->

The _mz_zip_ object allows for the reading and writing of the a zip file and its entries.

- [Archive](#archive)
  - [mz_zip_create](#mz_zip_create)
  - [mz_zip_delete](#mz_zip_delete)
  - [mz_zip_open](#mz_zip_open)
  - [mz_zip_close](#mz_zip_close)
  - [mz_zip_get_comment](#mz_zip_get_comment)
  - [mz_zip_set_comment](#mz_zip_set_comment)
  - [mz_zip_get_version_madeby](#mz_zip_get_version_madeby)
  - [mz_zip_set_version_madeby](#mz_zip_set_version_madeby)
  - [mz_zip_set_recover](#mz_zip_set_recover)
  - [mz_zip_set_data_descriptor](#mz_zip_set_data_descriptor)
  - [mz_zip_get_stream](#mz_zip_get_stream)
  - [mz_zip_set_cd_stream](#mz_zip_set_cd_stream)
  - [mz_zip_get_cd_mem_stream](#mz_zip_get_cd_mem_stream)
  - [mz_zip_set_number_entry](#mz_zip_set_number_entry)
  - [mz_zip_get_number_entry](#mz_zip_get_number_entry)
  - [mz_zip_set_disk_number_with_cd](#mz_zip_set_disk_number_with_cd)
  - [mz_zip_get_disk_number_with_cd](#mz_zip_get_disk_number_with_cd)
- [Entry I/O](#entry-io)
  - [mz_zip_entry_is_open](#mz_zip_entry_is_open)
  - [mz_zip_entry_read_open](#mz_zip_entry_read_open)
  - [mz_zip_entry_read](#mz_zip_entry_read)
  - [mz_zip_entry_read_close](#mz_zip_entry_read_close)
  - [mz_zip_entry_write_open](#mz_zip_entry_write_open)
  - [mz_zip_entry_write](#mz_zip_entry_write)
  - [mz_zip_entry_write_close](#mz_zip_entry_write_close)
  - [mz_zip_entry_seek_local_header](#mz_zip_entry_seek_local_header)
  - [mz_zip_entry_close_raw](#mz_zip_entry_close_raw)
  - [mz_zip_entry_close](#mz_zip_entry_close)
- [Entry Enumeration](#entry-enumeration)
  - [mz_zip_entry_is_dir](#mz_zip_entry_is_dir)
  - [mz_zip_entry_is_symlink](#mz_zip_entry_is_symlink)
  - [mz_zip_entry_get_info](#mz_zip_entry_get_info)
  - [mz_zip_entry_get_local_info](#mz_zip_entry_get_local_info)
  - [mz_zip_get_entry](#mz_zip_get_entry)
  - [mz_zip_goto_entry](#mz_zip_goto_entry)
  - [mz_zip_goto_first_entry](#mz_zip_goto_first_entry)
  - [mz_zip_goto_next_entry](#mz_zip_goto_next_entry)
  - [mz_zip_locate_entry](#mz_zip_locate_entry)
  - [mz_zip_locate_first_entry](#mz_zip_locate_first_entry)
  - [mz_zip_locate_next_entry](#mz_zip_locate_next_entry)
- [System Attributes](#system-attributes)
  - [mz_zip_attrib_is_dir](#mz_zip_attrib_is_dir)
  - [mz_zip_attrib_is_symlink](#mz_zip_attrib_is_symlink)
  - [mz_zip_attrib_convert](#mz_zip_attrib_convert)
  - [mz_zip_attrib_posix_to_win32](#mz_zip_attrib_posix_to_win32)
  - [mz_zip_attrib_win32_to_posix](#mz_zip_attrib_win32_to_posix)
- [Extrafield](#extrafield)
  - [mz_zip_extrafield_find](#mz_zip_extrafield_find)
  - [mz_zip_extrafield_contains](#mz_zip_extrafield_contains)
  - [mz_zip_extrafield_read](#mz_zip_extrafield_read)
  - [mz_zip_extrafield_write](#mz_zip_extrafield_write)
- [Time/Date](#timedate)
  - [mz_zip_dosdate_to_tm](#mz_zip_dosdate_to_tm)
  - [mz_zip_dosdate_to_time_t](#mz_zip_dosdate_to_time_t)
  - [mz_zip_time_t_to_tm](#mz_zip_time_t_to_tm)
  - [mz_zip_time_t_to_dos_date](#mz_zip_time_t_to_dos_date)
  - [mz_zip_tm_to_dosdate](#mz_zip_tm_to_dosdate)
  - [mz_zip_ntfs_to_unix_time](#mz_zip_ntfs_to_unix_time)
  - [mz_zip_unix_to_ntfs_time](#mz_zip_unix_to_ntfs_time)
- [Path](#path)
  - [mz_zip_path_compare](#mz_zip_path_compare)
- [String](#string)
  - [mz_zip_get_compression_method_string](#mz_zip_get_compression_method_string)

## Archive

### mz_zip_create

Creates a _mz_zip_ instance and returns its pointer.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to store the _mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|void *|Pointer to the _mz_zip_ instance|

**Example**
```
void *zip_handle = NULL;
mz_zip_create(&zip_handle);
```

### mz_zip_delete

Deletes a _mz_zip_ instance and resets its pointer to zero.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
void *zip_handle = NULL;
mz_zip_create(&zip_handle);
mz_zip_delete(&zip_handle);
```

### mz_zip_open

Opens a zip file given a stream.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void *|stream|_mz_stream_ instance|
|int32_t|mode|Open mode (See [MZ_OPEN_MODE](mz_open_mode.md))|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *zip_handle = NULL;

// TODO: Create stream

mz_zip_create(&zip_handle);
err = mz_zip_open(zip_handle, stream, MZ_OPEN_MODE_READ);
if (err != MZ_OK)
    printf("Error opening zip file for reading\n");
else
    mz_zip_close(zip_handle);
mz_zip_delete(&zip_handle);

// TODO: Delete stream
```

### mz_zip_close

Close a zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
// TODO: Create and open mz_zip instance
mz_zip_close(zip_handle);
mz_zip_delete(&zip_handle);
```

### mz_zip_get_comment

Gets the zip file's global comment string.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|const char **|comment|Pointer to null-terminated comment character array|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *global_comment = NULL;
if (mz_zip_get_comment(zip_handle, &global_comment) == MZ_OK)
    printf("Zip file global comment: %s\n", global_comment);
else
    printf("Zip file does not contain a global comment\n");
```

### mz_zip_set_comment

Sets the zip file's global comment string when the zip file is opened for writing. According to the zip file specification, each zip file can store a global comment with a maximum length of _UINT16_MAX_ or 65535 characters.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|const char *|comment|Null-terminated comment character array|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *global_comment = "Hi! This is my zip file.";
if (mz_zip_set_comment(zip_handle, global_comment) == MZ_OK)
    printf("Successfully set the zip file's global comment\n");
```

### mz_zip_get_version_madeby

Gets the zip file's version information.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint16_t *|version_madeby|Pointer to version value (See [PKWARE zip app note](zip/appnote.txt) 4.4.2)|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
uint16_t version_madeby = 0;
if (mz_zip_get_version_madeby(zip_handle, &version_madeby) == MZ_OK) {
    printf("Zip eocd version made by: %04x\n", version_madeby);
    printf("Zip eocd host system: %d\n", MZ_HOST_SYSTEM(version_madeby));
    printf("Zip eocd app note version: %d\n", (version_madeby & 0xFF));
}
```

### mz_zip_set_version_madeby

Sets the zip file's version information. The application that original wrote the zip file would have set this value to indicate what operating system and version of the zip specification that was used when making it. This version is stored in the end of central directory record; there is another similar _version_madeby_ stored for each zip entry.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint16_t|version_madeby|Version value (See [PKWARE zip app note](zip/appnote.txt) 4.4.2)|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
mz_zip_set_version_madeby(zip_handle, MZ_VERSION_MADEBY);
// MZ_VERSION_MADEBY is the macro use to represent the version made by for the host system
uint16_t custom_version_madeby = ((MZ_HOST_SYSTEM_WINDOWS_NTFS << 8) | 45)
mz_zip_set_version_madeby(zip_handle, custom_version_madeby);
```

### mz_zip_set_recover

Sets the ability to recover/repair the central directory. When the central directory is damaged or incorrectly written, it may be possible to recover it by reading the local file headers. Reading all of the local file headers can be a disk intensive operation and take significant time for large files.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint8_t|recover|Set to 1 to enable central directory recover, set to 0 otherwise.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *zip_handle = NULL;
mz_zip_create(&zip_handle);
// Enable central directory recover/repair
if (mz_zip_set_recover(zip_handle, 1) == MZ_OK)
    printf("Central directory recovery enabled if necessary\n");
```

### mz_zip_set_data_descriptor

Sets wehther or not zip file entries will be written with a data descriptor. When data descriptor writing is enabled it will zero out the crc32, compressed size, and uncompressed size in the local header. By default data descriptor writing is enabled and disabling it will cause zip file entry writing to seek backwards to fill in these values after writing the compressed data.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint8_t|data_descriptor|Set to 1 to enable data descriptor writing, set to 0 otherwise.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *zip_handle = NULL;
mz_zip_create(&zip_handle);
// Enable data descriptor writing for zip entries
if (mz_zip_set_data_descriptor(zip_handle, 0) == MZ_OK)
    printf("Local file header entries will be written with crc32 and sizes\n");
```

### mz_zip_get_stream

Gets the _mz_stream_ handle used in the call to _mz_zip_open_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void **|stream|Pointer to _mz_stream_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *zip_handle = NULL;
mz_zip_create(&zip_handle);
if (mz_zip_open(zip_handle, stream_handle, MZ_OPEN_MODE_READ) == MZ_OK) {
    void *stream2_handle = NULL;
    mz_zip_get_stream(zip_handle, &stream2_handle);
    assert(stream_handle == stream2_handle);
}
```

### mz_zip_set_cd_stream

Sets the stream to use for reading the central directory.

The central directory stream might not be the same as the stream used for reading local file entries. In the case of split disks, they are different and handles to both are open at the same time to keep from opening and closing streams while reading entries.

This function is used when encrypting the central directory, it is decrypted and extracted into a memory stream, and that memory stream is set as the central directory stream.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|int64_t|cd_start_pos|Start position of central directory in the stream|
|void *|cd_stream|_mz_stream_ instance to use for reading central directory. The cd_stream must be valid as long as it is being used by _mz_zip_.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *cd_mem_stream = NULL;
mz_stream_mem_create(&cd_mem_stream);
// TODO: Write central directory to memory stream
mz_zip_set_cd_stream(zip_handle, 0, cd_mem_stream);
```

### mz_zip_get_cd_mem_stream

Gets the stream used to store the central directory in memory for writing.

When writing to a zip file the central directory is written to this memory stream and written after all entries are written. When a zip file is opened for appending, the central directory is stored in memory while the appending new zip entries.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void *|cd_stream|Pointer to _mz_stream_ instance that is used for writing the central directory.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
// TODO: Open zip file for writing and write at least one entry
void *cd_mem_stream = NULL;
if (mz_zip_get_cd_mem_stream(zip_handle, &cd_mem_stream) == MZ_OK) {
    int64_t org_position = mz_stream_tell(cd_mem_stream);
    mz_stream_seek(cd_mem_stream, 0, MZ_SEEK_END);
    int64_t cd_length = mz_stream_tell(cd_mem_stream);
    mz_stream_seek(cd_mem_stream, org_position, MZ_SEEK_SET);
    printf("Length of central directory to write: %d\n", cd_length);
}
```

### mz_zip_set_number_entry

Sets the total number of entries in the zip file. Useful when loading central directory manually via _mz_zip_set_cd_stream_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint64_t|number_entry|Total number of entries in central directory|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
// TODO: Load central directory into memory stream
mz_zip_set_cd_stream(zip_handle, 0, cd_mem_stream);
mz_zip_set_number_entry(zip_handle, 10);
```

### mz_zip_get_number_entry

Gets the total number of entries in the zip file after it has been opened.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint64_t|number_entry|Pointer to store total number of entries in central directory|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
uint64_t number_entry = 0;

// TODO: Open zip file

if (mz_zip_get_number_entry(zip_handle, &number_entry) == MZ_OK)
    printf("Total number of entries in zip file %d\n", number_entry);
```

### mz_zip_set_disk_number_with_cd

Sets the disk number containing the central directory record. Useful for when loading the central directory manually using _mz_zip_set_cd_stream_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint32_t|disk_number_with_cd|Disk number containing the central directory|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
// TODO: Load central directory into memory stream
mz_zip_set_cd_stream(zip_handle, 0, cd_mem_stream);
mz_zip_set_number_entry(zip_handle, 10);
mz_zip_set_disk_number_with_cd(zip_handle, 0);
```
### mz_zip_get_disk_number_with_cd

Gets the disk number containing the central directory.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint32_t *|disk_number_with_cd|Pointer to store disk number containing the central directory|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
uint32_t disk_number_with_cd = 0;
// TODO: Open zip file
if (mz_zip_get_disk_number_with_cd(zip_handle, &disk_number_with_cd) == MZ_OK)
    printf("Disk number containing cd: %d\n", disk_number_with_cd);
```

## Entry I/O

### mz_zip_entry_is_open

Gets whether or not a zip entry is open for reading or writing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if open|

**Example**
```
// TODO: Open zip file
if (mz_zip_entry_is_open(zip_handle) == MZ_OK)
    printf("Zip entry is open for reading or writing\n");
```

### mz_zip_entry_read_open

Opens for reading the current entry in the zip file. To navigate to an entry use _mz_zip_goto_first_entry_, _mz_zip_goto_next_entry_, or _mz_zip_locate_entry_.

Normally, when reading from a zip entry, the data will be automatically decrypted and decompressed. To read the raw zip entry data, set the raw parameter to 1. This is useful if you want access to the raw gzip data (assuming the entry is gzip compressed).

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint8_t|raw|Open for raw reading if 1.|
|const char *|password|Null-terminated password character array, or NULL if no password needed for reading.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if entry opened for reading.|

**Example**
```
err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK)
    err = mz_zip_entry_read_open(zip_handle, 0, NULL);
if (err == MZ_OK)
    printf("Zip entry open for reading\n");
```

### mz_zip_entry_read

Reads bytes from the current entry in the zip file. The data returned in the buffer will be the decrypted and decompressed bytes unless the entry is opened for raw data reading.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void *|buf|Read buffer array|
|int32_t|len|Maximum bytes to read.|

**Return**
|Type|Description|
|-|-|
|int32_t|If < 0 then [MZ_ERROR](mz_error.md) code, otherwise number of bytes read. When there are no more bytes left to read then 0 is returned.|

**Example**
```
int32_t bytes_read;
int32_t err = MZ_OK;
char buf[4096];
do {
    bytes_read = mz_zip_entry_read(zip_handle, buf, sizeof(buf));
    if (bytes_read < 0) {
        err = bytes_read;
    }
    // TODO: Do something with buf bytes
} while (err == MZ_OK && bytes_read > 0);
```

### mz_zip_entry_read_close

Closes the current entry in the zip file for reading and returns the data descriptor values if the zip entry has the data descriptor flag set. If the data descriptor values are not necessary, _mz_zip_entry_close_ can be used instead.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint32_t *|crc32|Pointer to store crc32 value from data descriptor. Use NULL if not needed.|
|int64_t *|compressed_size|Pointer to store compressed size from data descriptor. Use NULL if not needed.|
|int64_t *|uncompressed_size|Pointer to store uncompressed size from data descriptor. Use NULL if not needed.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
uint32_t crc32 = 0;
int64_t compressed_size = 0;
int64_t uncompressed_size = 0;
int32_t err = mz_zip_entry_read_close(zip_handle, &crc32, &compressed_size, &uncompressed_size);
if (err == MZ_OK) {
    printf("Zip entry crc32: %08x\n", crc32);
    printf("Zip entry compressed size: %lld\n", compressed_size);
    printf("Zip entry uncompressed size: %lld\n", uncompressed_size);
} else {
    printf("Unknown error closing zip entry for reading (%d)\n", err);
}
```

### mz_zip_entry_write_open

Opens for a new entry in the zip file for writing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|const mz_zip_file *|file_info|Pointer to [_mz_zip_file_](mz_zip_file.md) structure containing information about entry being added.|
|int16_t|compress_level|Compression level 0-9. Higher is better compression. (See [MZ_COMPRESS_LEVEL](mz_compress_level.md))|
|uint8_t|raw|Write raw data if 1, otherwise the data will be compressed and encrypted based on supplied parameters.|
|const char *|password|Null-terminated password character array. Use NULL if encryption not required.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if entry opened for writing.|

**Example**

See _mz_zip_writer_add_file_ for a full example on how to populate the [mz_zip_file](mz_zip_file.md) structure.
```
mz_zip_file file_info;

memset(&file_info, 0, sizeof(file_info));

file_info.version_madeby = MZ_VERSION_MADEBY;
file_info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
file_info.filename = "myfile.txt";
file_info.flag = MZ_ZIP_FLAG_UTF8;
...

err = mz_zip_entry_write_open(zip_handle, &file_info, 0, NULL);
if (err == MZ_OK)
    printf("Zip entry open for writing\n");
```

### mz_zip_entry_write

Write data to the current entry in the zip file that was opened for writing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void *|buf|Write buffer array|
|int32_t|len|Maximum bytes to write.|

**Return**
|Type|Description|
|-|-|
|int32_t|If < 0 then [MZ_ERROR](mz_error.md) code, otherwise number of bytes written.|

**Example**
```
char buf[4096];
int32_t bytes_to_write = sizeof(buf);
int32_t bytes_written = 0;
int32_t total_bytes_written = 0;
int32_t err = MZ_OK;
// Fill buf with x'es
memset(buf, 'x', sizeof(buf));
do {
    bytes_written = mz_zip_entry_write(zip_handle, buf + total_bytes_written,
        bytes_to_write);
    if (bytes_written < 0) {
        err = bytes_written;
    } else {
        total_bytes_written += bytes_written;
        bytes_to_write -= bytes_written;
    }
} while (err == MZ_OK && bytes_to_write > 0);
```

### mz_zip_entry_write_close

Closes the current entry in the zip file for writing and allows setting the data descriptor values if the zip entry has the data descriptor flag set. If the data descriptor values are not necessary, _mz_zip_entry_close_ can be used instead.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|uint32_t|crc32|Crc32 value to store in the data descriptor.|
|int64_t|compressed_size|Compressed size to store in the data descriptor.|
|int64_t|uncompressed_size|Uncompressed size to store in the data descriptor.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
uint32_t crc32 = 0;
int64_t compressed_size = 0;
int64_t uncompressed size = 0;
// TODO: Calculate crc32, compressed size, and uncompressed size
int32_t err = mz_zip_entry_write_close(zip_handle, crc32, compressed_size, uncompressed_size);
if (err == MZ_OK)
    printf("Zip file entry closed for writing\n");
```

### mz_zip_entry_seek_local_header
Seeks to the local header for the entry.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK)
    err = mz_zip_entry_seek_local_header(zip_handle);
if (err == MZ_OK) {
    void *stream = NULL;
    mz_zip_get_stream(zip_handle, &stream);
    int64_t position = mz_stream_tell(stream);
    printf("Position of local header of first entry: %lld\n", position);
}
```

### mz_zip_entry_close_raw

Closes the current entry in the zip file. To be used to close an entry that has been opened for reading or writing in raw mode.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|int64_t|uncompressed_size|Uncompressed size to store in the data descriptor. Only used when entry was opened for writing.|
|uint32_t|crc32|Crc32 value to store in the data descriptor. Only used when entry was opened for writing.|

Compressed size is already known through calls to _mz_zip_entry_write_.

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
#include <zlib.h>
int32_t err = mz_zip_entry_read_write(zip_handle, &file_info, 1, NULL);
if (err == MZ_OK) {
    z_stream zs;
    int32_t my_string_crc32;
    int32_t my_string_size;
    char buf[4096];
    char *my_string = "mystring";

    my_string_size = strlen(my_string);
    my_string_crc32 = crc32(0, my_string, my_string_size);

    memset(&zs, 0, sizeof(zs));

    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = my_string_size;
    zs.next_in = my_string;
    zs.avail_out = sizeof(buf);
    zs.next_out = buf;

    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
    err = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    mz_zip_entry_write(zip_handle, buf, zs.total_out);
    mz_zip_entry_close_raw(zip_handle, my_string_size, my_string_crc32);
}
```

### mz_zip_entry_close

Closes the current entry in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
int32_t err = mz_zip_entry_close(zip_handle);
if (err == MZ_OK)
    printf("Zip entry closed\n");
```

## Entry Enumeration

### mz_zip_entry_is_dir

When enumerating zip entries, returns whether or not the current entry is a directory. This function accounts for the various directory attribute values on each OS.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if entry is a directory.|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK) {
    if (mz_zip_entry_is_dir(zip_handle) == MZ_OK) {
        printf("First entry in zip file is a directory\n");
    }
}
```

### mz_zip_entry_is_symlink

When enumerating zip entries, returns whether or not the current entry is a symbolic link. This function accounts for the various symbolic link attribute values on each OS.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if entry is a symbolic link.|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK) {
    if (mz_zip_entry_is_symlink(zip_handle) == MZ_OK) {
        printf("First entry in zip file is a symbolic link\n");
    }
}
```

### mz_zip_entry_get_info

Gets central directory file information about the current entry in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|mz_zip_file **|file_info|Pointer to _mz_zip_file_ structure. Pointer is only valid for the while the entry is the current entry.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK) {
    mz_zip_file *file_info = NULL;
    err = mz_zip_entry_get_info(zip_handle, &file_info);
    if (err == MZ_OK) {
        printf("Central directory entry filename: %s\n", file_info->filename);
    }
}
```

### mz_zip_entry_get_local_info

Gets local file header file information about the current entry in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|mz_zip_file **|file_info|Pointer to _mz_zip_file_ structure. Pointer is only valid for the while the entry is the current entry.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK) {
    mz_zip_file *local_file_info = NULL;
    err = mz_zip_entry_get_info(zip_handle, &local_file_info);
    if (err == MZ_OK) {
        printf("Local header entry filename: %s\n", local_file_info->filename);
    }
}
```

### mz_zip_get_entry

Returns the offset of the current entry in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int64_t|Byte position in zip file, if < 0, then [MZ_ERROR](mz_error.md).|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK) {
    int64_t entry_offset = mz_zip_get_entry(zip_handle);
    if (entry_offset >= 0) {
        printf("Entry offset %lld\n", entry_offset);
    }
}
```

### mz_zip_goto_entry

Manually set the central directory stream position for and read entry.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|int64_t|cd_pos|Position in the central directory stream|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
// Read the entry at position 0 in central directory stream
mz_zip_goto_entry(zip_handle, 0);
```

### mz_zip_goto_first_entry

Go to the first entry in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if no more entries.|

**Example**
```
int32_t err = mz_zip_goto_first_entry(zip_handle);
if (err == MZ_OK) {
    mz_zip_file *file_info = NULL;
    err = mz_zip_entry_get_info(zip_handle, &file_info);
    if (err == MZ_OK) {
        printf("First entry is %s\n", file_info->filename);
    }
}
```

### mz_zip_goto_next_entry

Go to the next entry in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if no more entries.|

**Example**
```
int32_t i = 0;
int32_t err = mz_zip_goto_first_entry(zip_handle);
while (err == MZ_OK) {
    mz_zip_file *file_info = NULL;
    err = mz_zip_entry_get_info(zip_handle, &file_info);
    if (err != MZ_OK) {
        printf("Failed to get entry %d info\n", i);
        break;
    }
    printf("Entry %d is %s\n", i, file_info->filename);
    err = mz_zip_goto_next_entry(zip_handle);
}
```

### mz_zip_locate_entry

Locate the entry with the specified name in the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|const char *|filename|Filename character array|
|uint8_t|ignore_case|Ignore case during lookup if 1.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if not found.|

**Example**
```
const char *search_path = "test.txt";
int32_t err = mz_zip_locate_entry(zip_handle, search_path, 0);
if (err == MZ_OK) {
    printf("%s was found\n", search_path);
else
    printf("%s was not found\n", search_path);
```

### mz_zip_locate_first_entry

Locate the first matching entry based on a match callback.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void *|userdata|User pointer|
|mz_zip_locate_entry_cb|cb|Callback to locate filter function|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if not found.|

**Example**
```
static int32_t locate_a_entries_cb(void *handle, void *userdata, mz_zip_file *file_info) {
    if (file_info->filename[0] == 'a') {
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}
int32_t err = mz_zip_locate_first_entry(zip_handle, NULL, locate_a_entries_cb);
if (err == MZ_OK) {
    mz_zip_file *file_info = NULL;
    err = mz_zip_entry_get_info(zip_handle, &file_info);
    if (err == MZ_OK) {
        printf("First entry beginning with a is %s\n", file_info->filename);
    }
} else {
    printf("No entries beginning with a found\n");
}
```

### mz_zip_locate_next_entry

Locate the next matching entry based on a match callback.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_ instance|
|void *|userdata|User pointer|
|mz_zip_locate_entry_cb|cb|Callback to locate filter function|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if not found.|

**Example**
```
static int32_t locate_a_entries_cb(void *handle, void *userdata, mz_zip_file *file_info) {
    if (file_info->filename[0] == 'a') {
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}
int32_t err = mz_zip_locate_first_entry(zip_handle, NULL, locate_a_entries_cb);
if (err == MZ_END_OF_LIST) {
    printf("No entries beginning with a found\n");
    return;
}
while (err == MZ_OK) {
    mz_zip_file *file_info = NULL;
    err = mz_zip_entry_get_info(zip_handle, &file_info);
    if (err != MZ_OK) {
        printf("Error getting entry info\n");
        break;
    }
    printf("Entry beginning with a is %s\n", file_info->filename);
    err = mz_zip_locate_next_entry(zip_handle, 0, locate_a_entries_cb);
}
```

## System Attributes

### mz_zip_attrib_is_dir

Checks to see if the attribute is a directory based on platform.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint32_t|attrib|External file attributes|
|int32_t|version_madeby|Version made by value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if is a directory.|

**Example**
```
mz_zip_file *file_info = NULL;
if (mz_zip_entry_get_info(zip_handle, &file_info) == MZ_OK) {
    if (mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby) == MZ_OK) {
        printf("Entry is a directory\n");
    } else {
        printf("Entry is not a directory\n");
    }
}
```

### mz_zip_attrib_is_symlink

Checks to see if the attribute is a symbolic link based on platform.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint32_t|attrib|External file attributes|
|int32_t|version_madeby|Version made by value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if is a directory.|

**Example**
```
mz_zip_file *file_info = NULL;
if (mz_zip_entry_get_info(zip_handle, &file_info) == MZ_OK) {
    if (mz_zip_attrib_is_symlink(file_info->external_fa, file_info->version_madeby) == MZ_OK) {
        printf("Entry is a symbolic link\n");
    } else {
        printf("Entry is not a symbolic link\n");
    }
}
```

### mz_zip_attrib_convert

Converts file attributes from one host system to another.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint8_t|src_sys|Source system (See [MZ_HOST_SYSTEM](mz_host_system.md))|
|uint32_t|src_attrib|Source system attribute value|
|uint8_t|target_sys|Target system (See [MZ_HOST_SYSTEM](mz_host_system.md))|
|uint32_t *|target_attrib|Pointer to store target system attribute value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if converted successfully.|

**Example**
```
uint32_t target_attrib = 0;
// Windows FILE_ATTRIBUTE_DIRECTORY (0x10)
if (mz_zip_attrib_convert(MZ_HOST_SYSTEM_WINDOWS_NTFS, 0x10, MZ_HOST_SYSTEM_UNIX, &target_attrib) == MZ_OK) {
    printf("Unix file attributes: %08x\n", target_attrib);
    if (S_ISDIR(target_attribute)) {
        printf("Unix attribute is a directory\n");
    }
}
```

### mz_zip_attrib_posix_to_win32

Converts posix file attributes to win32 file attributes.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint32_t|posix_attrib|Posix attributes value|
|uint32_t *|win32_attrib|Pointer to store windows attributes value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if converted successfully.|

**Example**
```
uint32_t posix_attrib = 0x00008124;
uint32_t win32_attrib = 0;
if (mz_zip_attrib_posix_to_win32(posix_attrib, &win32_attrib) == MZ_OK) {
    printf("Win32 file system attributes: %08x\n", win32_attrib);
    // Windows FILE_ATTRIBUTE_READONLY (0x01)
    if ((win32_attrib & 0x01) != 0) {
        printf("Win32 attribute is readonly\n");
    }
}
```

### mz_zip_attrib_win32_to_posix

Converts win32 file attributes to posix file attributes.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint32_t|win32_attrib|Windows attributes value|
|uint32_t *|posix_attrib|Pointer to store posix attributes value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if converted successfully.|

**Example**
```
uint32_t posix_attrib = 0; // Read only
uint32_t win32_attrib = 0x01; // FILE_ATTRIBUTE_READONLY;
if (mz_zip_attrib_win32_to_posix(win32_attrib), &posix_attrib) == MZ_OK) {
    printf("Posix file system attributes: %08x\n", posix_attrib);
    if ((posix_attrib & 0000222) == 0) {
        printf("Posix attribute is readonly\n");
    }
}
```

## Extrafield

### mz_zip_extrafield_find

Seeks using a _mz_stream_ to an extra field by its type and returns its length.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|stream|_mz_stream_ instance|
|uint16_t|type|Extra field type indentifier (See [PKWARE zip app note](zip/appnote.iz.txt) section 4.5.2)|
|int32_t|max_seek|Maximum length to search for extrafield|
|uint16_t *|length|Pointer to extra field length|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if found, MZ_EXIST_ERROR if not found.|

**Example**
```
void *file_extra_stream = NULL;
mz_zip_file *file_info = NULL;
uint16_t extrafield_length = 0;

mz_zip_entry_get_info(zip_handle, &file_info);

mz_stream_mem_create(&file_extra_stream);
mz_stream_mem_set_buffer(file_extra_stream, (void *)file_info->extrafield,
    file_info->extrafield_size);

if (mz_zip_extrafield_find(file_extra_stream, MZ_ZIP_EXTENSION_AES, INT32_MAX, &extrafield_length) == MZ_OK)
    printf("Found AES extra field, length %d\n", extrafield_length);
else
    printf("Unable to find AES extra field in zip entry\n");

mz_stream_mem_delete(&file_extra_stream);
```

### mz_zip_extrafield_contains

Searchs a buffer to determine whether an extrafield exists and returns its length if found.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const uint8_t *|extrafield|Extra field buffer array|
|int32_t|extrafield_size|Maximim buffer bytes|
|uint16_t|type|Extrafield type identifier (See [PKWARE zip app note](zip/appnote.iz.txt) section 4.5.2)|
|uint16_t *|length|Pointer to store extrafield length|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if found, MZ_EXIST_ERROR if not found.|

**Example**
```
mz_zip_file *file_info = NULL;
uint16_t extrafield_length = 0;

mz_zip_entry_get_info(zip_handle, &file_info);
if (mz_zip_extrafield_contains(file_info->extrafield, file_info->extrafield_size, MZ_ZIP_EXTENSION_AES, &extrafield_length) == MZ_OK)
    printf("Found AES extra field, length %d\n", extrafield_length);
else
    printf("Unable to find AES extra field in zip entry\n");

```

### mz_zip_extrafield_read

Reads an extrafield header from a stream.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|stream|_mz_stream_ instance|
|uint16_t *|type|Pointer to store extrafield type identifier (See [PKWARE zip app note](zip/appnote.iz.txt) section 4.5.2)|
|uint16_t *|length|Pointer to store extrafield length|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if extrafield read.|

**Example**
```
void *file_extra_stream = NULL;
mz_zip_file *file_info = NULL;
uint16_t extrafield_type = 0;
uint16_t extrafield_length = 0;

mz_stream_mem_create(&file_extra_stream);
mz_stream_mem_set_buffer(file_extra_stream, (void *)file_info->extrafield,
    file_info->extrafield_size);

while (mz_zip_extrafield_read(field_extra_stream, &extrafield_type, &extrafield_length) == MZ_OK) {
    printf("Extra field type: %04x\n", extrafield_type);
    printf("Extra field length: %d\n", extrafield_length);
}

mz_stream_mem_delete(&file_extra_stream);
```

### mz_zip_extrafield_write

Writes an extrafield header to a stream.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|stream|_mz_stream_ instance|
|uint16_t|type|Extrafield type identifier (See [PKWARE zip app note](zip/appnote.iz.txt) section 4.5.2)|
|uint16_t|length|Extrafield length|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
#define MY_CUSTOM_FIELD_TYPE (0x8080)
#define MY_CUSTOM_FIELD_LENGTH (sizeof(uint32_t))
#define MY_CUSTOM_FIELD_VALUE (1)

if (mz_zip_extrafield_write(field_extra_stream, MY_CUSTOM_FIELD_TYPE, MY_CUSTOM_FIELD_LENGTH) == MZ_OK)
    mz_stream_write_uint32(field_extra_stream, MY_CUSTOM_FIELD_VALUE);
```

## Time/Date

### mz_zip_dosdate_to_tm

Convert dos date/time format to struct tm.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint64_t|dos_date|Dos date/time timestamp|
|struct tm *|ptm|Pointer to tm structure|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
struct tm tmu_date = { 0 };
uint32_t dos_date = 0x50454839;
mz_zip_dosdate_to_tm(dos_date, &tmu_date);
printf("Time: %02d/%02d/%d %02d:%02d:%02d\n",
    (uint32_t)tmu_date.tm_mon + 1, (uint32_t)tmu_date.tm_mday,
    (uint32_t)tmu_date.tm_year % 100,
    (uint32_t)tmu_date.tm_hour, (uint32_t)tmu_date.tm_min,
    (uint32_t)tmu_date.tm_sec);
```

### mz_zip_dosdate_to_time_t

Convert dos date/time format to unix timestamp.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint64_t|dos_date|Dos date/time timestamp|

**Return**
|Type|Description|
|-|-|
|time_t|Unix timestamp, 0 if not converted.|

**Example**
```
uint32_t dos_date = 0x50454839;
time_t unix_time = mz_zip_dosdate_to_time_t(unix_time);
// Unix date/time = 1580922110
printf("Unix date/time: %lld\n", unix_time);
```

### mz_zip_time_t_to_tm

Convert unix timestamp to time struct.

**Arguments**
|Type|Name|Description|
|-|-|-|
|time_t|unix_time|Unix timestamp|
|struct tm *|ptm|Pointer to tm structure|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
struct tm tmu_date = { 0 };
time_t unix_time = 1580922110;
mz_zip_time_t_to_tm(unix_time, &tmu_date);
printf("Time: %02d/%02d/%d %02d:%02d:%02d\n",
    (uint32_t)tmu_date.tm_mon + 1, (uint32_t)tmu_date.tm_mday,
    (uint32_t)tmu_date.tm_year % 100,
    (uint32_t)tmu_date.tm_hour, (uint32_t)tmu_date.tm_min,
    (uint32_t)tmu_date.tm_sec);
```

### mz_zip_time_t_to_dos_date

Convert unix timestamp to dos date/time format.

**Arguments**
|Type|Name|Description|
|-|-|-|
|time_t|unix_time|Unix timestamp|

**Return**
|Type|Description|
|-|-|
|uint32_t|Dos/date timestamp, 0 if not converted.|

**Example**
```
time_t unix_time = 1580922110;
uint32_t dos_date = mz_zip_time_t_to_dos_date(unix_time);
// Dos date/time = 0x50454839
printf("Dos date/time: %08x\n", dos_date);
```

### mz_zip_tm_to_dosdate

Convert struct tm to dos date/time format.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const struct tm *|ptm|Pointer to tm structure|

**Return**
|Type|Description|
|-|-|
|uint32_t|Dos/date timestamp, 0 if not converted.|

**Example**
```
struct tm tmu_date = { 50, 1, 9, 5, 1, 120, 3, 35, 0 };
uint32_t dos_date = mz_zip_tm_to_dosdate(&tmu_date);
printf("Dos date/time: %08x\n", dos_date);
```

### mz_zip_ntfs_to_unix_time

Convert ntfs time to unix time.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint64_t|ntfs_time|Windows NTFS timestamp|
|time_t *|unix_time|Pointer to store Unix timestamp|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
time_t unix_time = 0;
uint64_t ntfs_time = 132253957100000000LL;
if (mz_zip_ntfs_to_unix_time(ntfs_time, &unix_time) == MZ_OK) {
    // Unix time = 1580922110
    printf("NTFS -> Unix: %lld -> %lld\n", ntfs_time, unix_time);
}
```

### mz_zip_unix_to_ntfs_time

Convert unix time to ntfs time.

**Arguments**
|Type|Name|Description|
|-|-|-|
|time_t|unix_time|Unix timestamp|
|uint64_t *|ntfs_time|Pointer to store Windows NTFS timestamp|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
time_t unix_time = 1580922110;
uint64_t ntfs_time = 0;
if (mz_zip_unix_to_ntfs_time(unix_time, &ntfs_time) == MZ_OK) {
    // NTFS time = 132253957100000000
    printf("Unix -> NTFS: %lld -> %lld\n", unix_time, ntfs_time);
}
```

## Path

### mz_zip_path_compare

Compare two paths without regard to slashes. Some zip files have paths with unix slashes and some zip files have paths containing windows slashes.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path1|First path to compare|
|const char *|path2|Second path to compare|
|uint8_t|ignore_case|Ignore case if 1.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
const char *search_path = "folder1/test1.txt";
mz_zip_file *file_info = NULL;
if (mz_zip_entry_get_info(zip_handle, &file_info) == MZ_OK) {
    if (mz_zip_path_compare(file_info->filename, search_path, 0) == MZ_OK)
        printf("Found %s\n", search_path);
    else
        printf("Not found %s\n", search_path);
}
```

## String

### mz_zip_get_compression_method_string

Gets a string representing the compression method.

**Arguments**
|Type|Name|Description|
|-|-|-|
|int32_t|compression_method|Compression method index|

**Return**
|Type|Description|
|-|-|
|const char *|String representing compression method or "?" if not found|

**Example**
```
const char *method = mz_zip_get_compression_method_string(MZ_ZIP_COMPRESS_METHOD_LZMA);
printf("Compression method %s\n", method);
```
