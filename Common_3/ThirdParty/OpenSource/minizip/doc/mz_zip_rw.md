## MZ_ZIP_RW <!-- omit in toc -->

The _mz_zip_reader_ and _mz_zip_writer_ objects allows you to easily extract or create zip files.

- [Reader Callbacks](#reader-callbacks)
  - [mz_zip_reader_overwrite_cb](#mz_zip_reader_overwrite_cb)
  - [mz_zip_reader_password_cb](#mz_zip_reader_password_cb)
  - [mz_zip_reader_progress_cb](#mz_zip_reader_progress_cb)
  - [mz_zip_reader_entry_cb](#mz_zip_reader_entry_cb)
- [Reader Open/Close](#reader-openclose)
  - [mz_zip_reader_is_open](#mz_zip_reader_is_open)
  - [mz_zip_reader_open](#mz_zip_reader_open)
  - [mz_zip_reader_open_file](#mz_zip_reader_open_file)
  - [mz_zip_reader_open_file_in_memory](#mz_zip_reader_open_file_in_memory)
  - [mz_zip_reader_open_buffer](#mz_zip_reader_open_buffer)
  - [mz_zip_reader_close](#mz_zip_reader_close)
- [Reader Entry Enumeration](#reader-entry-enumeration)
  - [mz_zip_reader_goto_first_entry](#mz_zip_reader_goto_first_entry)
  - [mz_zip_reader_goto_next_entry](#mz_zip_reader_goto_next_entry)
  - [mz_zip_reader_locate_entry](#mz_zip_reader_locate_entry)
- [Reader Entry](#reader-entry)
  - [mz_zip_reader_entry_open](#mz_zip_reader_entry_open)
  - [mz_zip_reader_entry_close](#mz_zip_reader_entry_close)
  - [mz_zip_reader_entry_read](#mz_zip_reader_entry_read)
  - [mz_zip_reader_entry_has_sign](#mz_zip_reader_entry_has_sign)
  - [mz_zip_reader_entry_sign_verify](#mz_zip_reader_entry_sign_verify)
  - [mz_zip_reader_entry_get_hash](#mz_zip_reader_entry_get_hash)
  - [mz_zip_reader_entry_get_first_hash](#mz_zip_reader_entry_get_first_hash)
  - [mz_zip_reader_entry_get_info](#mz_zip_reader_entry_get_info)
  - [mz_zip_reader_entry_is_dir](#mz_zip_reader_entry_is_dir)
  - [mz_zip_reader_entry_save](#mz_zip_reader_entry_save)
  - [mz_zip_reader_entry_save_process](#mz_zip_reader_entry_save_process)
  - [mz_zip_reader_entry_save_file](#mz_zip_reader_entry_save_file)
  - [mz_zip_reader_entry_save_buffer](#mz_zip_reader_entry_save_buffer)
  - [mz_zip_reader_entry_save_buffer_length](#mz_zip_reader_entry_save_buffer_length)
- [Reader Bulk Extract](#reader-bulk-extract)
  - [mz_zip_reader_save_all](#mz_zip_reader_save_all)
- [Reader Object](#reader-object)
  - [mz_zip_reader_set_pattern](#mz_zip_reader_set_pattern)
  - [mz_zip_reader_set_password](#mz_zip_reader_set_password)
  - [mz_zip_reader_set_raw](#mz_zip_reader_set_raw)
  - [mz_zip_reader_get_raw](#mz_zip_reader_get_raw)
  - [mz_zip_reader_get_zip_cd](#mz_zip_reader_get_zip_cd)
  - [mz_zip_reader_get_comment](#mz_zip_reader_get_comment)
  - [mz_zip_reader_set_recover](#mz_zip_reader_set_recover)
  - [mz_zip_reader_set_encoding](#mz_zip_reader_set_encoding)
  - [mz_zip_reader_set_sign_required](#mz_zip_reader_set_sign_required)
  - [mz_zip_reader_set_overwrite_cb](#mz_zip_reader_set_overwrite_cb)
  - [mz_zip_reader_set_password_cb](#mz_zip_reader_set_password_cb)
  - [mz_zip_reader_set_progress_cb](#mz_zip_reader_set_progress_cb)
  - [mz_zip_reader_set_progress_interval](#mz_zip_reader_set_progress_interval)
  - [mz_zip_reader_set_entry_cb](#mz_zip_reader_set_entry_cb)
  - [mz_zip_reader_get_zip_handle](#mz_zip_reader_get_zip_handle)
  - [mz_zip_reader_create](#mz_zip_reader_create)
  - [mz_zip_reader_delete](#mz_zip_reader_delete)
- [Writer Callbacks](#writer-callbacks)
  - [mz_zip_writer_overwrite_cb](#mz_zip_writer_overwrite_cb)
  - [mz_zip_writer_password_cb](#mz_zip_writer_password_cb)
  - [mz_zip_writer_progress_cb](#mz_zip_writer_progress_cb)
  - [mz_zip_writer_entry_cb](#mz_zip_writer_entry_cb)
- [Writer Open/Close](#writer-openclose)
  - [mz_zip_writer_is_open](#mz_zip_writer_is_open)
  - [mz_zip_writer_open](#mz_zip_writer_open)
  - [mz_zip_writer_open_file](#mz_zip_writer_open_file)
  - [mz_zip_writer_open_file_in_memory](#mz_zip_writer_open_file_in_memory)
  - [mz_zip_writer_close](#mz_zip_writer_close)
- [Writer Entry](#writer-entry)
  - [mz_zip_writer_entry_open](#mz_zip_writer_entry_open)
  - [mz_zip_writer_entry_close](#mz_zip_writer_entry_close)
  - [mz_zip_writer_entry_write](#mz_zip_writer_entry_write)
- [Writer Add/Compress](#writer-addcompress)
  - [mz_zip_writer_add](#mz_zip_writer_add)
  - [mz_zip_writer_add_process](#mz_zip_writer_add_process)
  - [mz_zip_writer_add_info](#mz_zip_writer_add_info)
  - [mz_zip_writer_add_buffer](#mz_zip_writer_add_buffer)
  - [mz_zip_writer_add_file](#mz_zip_writer_add_file)
  - [mz_zip_writer_add_path](#mz_zip_writer_add_path)
  - [mz_zip_writer_copy_from_reader](#mz_zip_writer_copy_from_reader)
- [Writer Object](#writer-object)
  - [mz_zip_writer_set_password](#mz_zip_writer_set_password)
  - [mz_zip_writer_set_comment](#mz_zip_writer_set_comment)
  - [mz_zip_writer_set_raw](#mz_zip_writer_set_raw)
  - [mz_zip_writer_get_raw](#mz_zip_writer_get_raw)
  - [mz_zip_writer_set_aes](#mz_zip_writer_set_aes)
  - [mz_zip_writer_set_compress_method](#mz_zip_writer_set_compress_method)
  - [mz_zip_writer_set_compress_level](#mz_zip_writer_set_compress_level)
  - [mz_zip_writer_set_zip_cd](#mz_zip_writer_set_zip_cd)
  - [mz_zip_writer_set_certificate](#mz_zip_writer_set_certificate)
  - [mz_zip_writer_set_overwrite_cb](#mz_zip_writer_set_overwrite_cb)
  - [mz_zip_writer_set_password_cb](#mz_zip_writer_set_password_cb)
  - [mz_zip_writer_set_progress_cb](#mz_zip_writer_set_progress_cb)
  - [mz_zip_writer_set_progress_interval](#mz_zip_writer_set_progress_interval)
  - [mz_zip_writer_set_entry_cb](#mz_zip_writer_set_entry_cb)
  - [mz_zip_writer_get_zip_handle](#mz_zip_writer_get_zip_handle)
  - [mz_zip_writer_create](#mz_zip_writer_create)
  - [mz_zip_writer_delete](#mz_zip_writer_delete)

## Reader Callbacks

### mz_zip_reader_overwrite_cb

Callback that called before an existing file is about to be overwritten. It can be set by calling _mz_zip_reader_set_overwrite_cb_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|Pointer that is passed to _mz_zip_reader_set_overwrite_cb_|
|mz_zip_file *|file_info|Zip entry|
|const char *|path|Target path on disk|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK to overwrite, MZ_EXIST_ERROR to skip.|

**Example**

See _minizip_extract_overwrite_cb_ callback in minizip.c.

### mz_zip_reader_password_cb

Callback that is called before a password is required to extract a password protected zip entry. It can be set by calling _mz_zip_reader_set_password_cb_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|Pointer that is passed to _mz_zip_reader_set_password_cb_|
|mz_zip_file *|file_info|Zip entry|
|char *|password|Password character array buffer|
|int32|max_password|Maximum password size|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
static int32_t example_password_cb(void *handle, void *userdata, mz_zip_file *file_info, char *password, int32_t max_password) {
    strncpy(password, "my password", max_password);
    return MZ_OK;
}
mz_zip_reader_set_password_cb(zip_reader, 0, example_password_cb);
```

### mz_zip_reader_progress_cb

Callback that is called to report extraction progress. This can be set by calling _mz_zip_reader_set_progress_cb_.

Progress calculation depends on whether or not raw data is being extracted. If raw data, then use `position / file_info->compressed_size` otherwise use `position / file_info->uncompressed_size`.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|Pointer that is passed to _mz_zip_reader_progress_cb_|
|mz_zip_file *|file_info|Zip entry|
|int64_t|position|File position.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**

See _minizip_extract_progress_cb_ in minizip.c.

### mz_zip_reader_entry_cb

Callback that is called when a new zip entry is starting extraction. It can be set by calling _mz_zip_reader_entry_cb_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|Pointer that is passed to _mz_zip_reader_entry_cb_|
|const char *|path|Target path on disk|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**

See _minizip_extract_entry_cb_ in minizip.c.

## Reader Open/Close

### mz_zip_reader_is_open

Checks to see if the zip file is open.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if open.|

**Example**
```
if (mz_zip_reader_is_open(zip_reader) == MZ_OK)
    printf("Zip file is open in reader\n");
```

### mz_zip_reader_open

 Opens zip file from stream.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|stream|_mz_stream_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
void *file_stream = NULL;
const char *path = "c:\\my.zip";

mz_zip_reader_create(&zip_reader);
mz_stream_os_create(&file_stream);

err = mz_stream_os_open(file_stream, path, MZ_OPEN_MODE_READ);
if (err == MZ_OK) {
    err = mz_zip_reader_open(zip_reader, file_stream);
    if (err == MZ_OK) {
        printf("Zip reader was opened %s\n", path);
        mz_zip_reader_close(zip_reader);
    }
}

mz_stream_os_delete(&file_stream);
mz_zip_reader_delete(&zip_reader);
```

### mz_zip_reader_open_file

Opens zip file from a file path.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|path|Path to zip file|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
const char *path = "c:\\my.zip";
mz_zip_reader_create(&zip_reader);
if (mz_zip_reader_open_file(zip_reader, path) == MZ_OK) {
    printf("Zip reader was opened %s\n", path);
    mz_zip_reader_close(zip_reader);
}
mz_zip_reader_delete(&zip_reader);
```

### mz_zip_reader_open_file_in_memory

Opens zip file from a file path into memory for faster access.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|path|Path to zip file|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
const char *path = "c:\\my.zip";
mz_zip_reader_create(&zip_reader);
if (mz_zip_reader_open_file_in_memory(zip_reader, path) == MZ_OK) {
    printf("Zip reader was opened in memory %s\n", path);
    mz_zip_reader_close(zip_reader);
}
mz_zip_reader_delete(&zip_reader);
```

### mz_zip_reader_open_buffer

Opens zip file from memory buffer.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint8_t *|buf|Buffer containing zip|
|int32_t|len|Length of buffer|
|int32_t|copy|Copy buffer internally if 1|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
uint8 *buffer = NULL;
int32 buffer_length = 0;
// TODO: Load zip file into memory buffer
mz_zip_reader_create(&zip_reader);
if (mz_zip_reader_open_buffer(zip_reader, buffer, buffer_length) == MZ_OK) {
    printf("Zip reader was opened from buffer\n");
    mz_zip_reader_close(zip_reader);
}
mz_zip_reader_delete(&zip_reader);
```

### mz_zip_reader_close

Closes the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
if (mz_zip_reader_close(zip_reader) == MZ_OK)
    printf("Zip reader closed\n");
```

## Reader Entry Enumeration

### mz_zip_reader_goto_first_entry

Goto the first entry in the zip file. If a pattern has been specified by calling _mz_zip_reader_set_pattern_, then it goes to the first entry matching the pattern.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if no more entries.|

**Example**
```
mz_zip_file *file_info = NULL;
if ((mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) &&
    (mz_zip_reader_entry_get_info(zip_reader, &file_info) == MZ_OK)) {
    printf("Zip first entry %s\n", file_info->filename);
}
```

### mz_zip_reader_goto_next_entry

Goto the next entry in the zip file. If a pattern has been specified by calling _mz_zip_reader_set_pattern_, then it goes to the next entry matching the pattern.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if no more entries.|

**Example**
```
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    do {
        mz_zip_file *file_info = NULL;
        if (mz_zip_reader_entry_get_info(zip_reader, &file_info) != MZ_OK) {
            printf("Unable to get zip entry info\n");
            break;
        }
        printf("Zip entry %s\n", file_info->filename);
    } while (mz_zip_reader_goto_next_entry(zip_reader) == MZ_OK);
}
```

### mz_zip_reader_locate_entry

Locates an entry by filename.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|filename|Filename to find|
|uint8_t|ignore_case|Ignore case during search if 1.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, MZ_END_OF_LIST if not found.|

**Example**
```
const char *search_filename = "test1.txt";
if (mz_zip_reader_locate_entry(zip_reader, search_filename, 1) == MZ_OK)
    printf("Found %s\n", search_filename);
else
    printf("Could not find %s\n", search_filename);
```

## Reader Entry

### mz_zip_reader_entry_open

Opens an entry for reading.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    if (mz_zip_reader_entry_open(zip_reader) == MZ_OK) {
        char buf[120];
        int32_t bytes_read = 0;
        bytes_read = mz_zip_reader_entry_read(zip_reader, buf, sizeof(buf));
        if (bytes_read > 0) {
            printf("Bytes read from entry %d\n", bytes_read);
        }
        mz_zip_reader_entry_close(zip_reader);
    }
}
```

### mz_zip_reader_entry_close

Closes an entry that has been opened for reading or writing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_zip_reader_entry_open(zip_reader) == MZ_OK) {
    if (mz_zip_reader_entry_close(zip_reader) == MZ_OK) {
        printf("Entry closed successfully\n");
    }
}
```

### mz_zip_reader_entry_read

Reads an entry after being opened.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|buf|Buffer to read into|
|int32_t|len|Maximum length of buffer to read into|

**Return**
|Type|Description|
|-|-|
|int32_t|If < 0 then [MZ_ERROR](mz_error.md) code, otherwise number of bytes read. When there are no more bytes left to read then 0 is returned.|

**Example**
```
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    if (mz_zip_reader_entry_open(zip_reader) == MZ_OK) {
        char buf[4096];
        int32_t bytes_read = 0;
        int32_t err = MZ_OK;
        do {
            bytes_read = mz_zip_reader_entry_read(zip_reader, buf, sizeof(buf));
            if (bytes_read < 0) {
                err = bytes_read;
                break;
            }
            printf("Bytes read from entry %d\n", bytes_read);
        } while (bytes_read > 0);
        mz_zip_reader_entry_close(zip_reader);
    }
}
```

### mz_zip_reader_entry_has_sign

Checks to see if the entry has a signature.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if has signature.|

**Example**
```
if (mz_zip_reader_entry_has_sign(zip_reader) == MZ_OK)
    printf("Entry has signature attached\n");
```

### mz_zip_reader_entry_sign_verify

Verifies a signature stored with the entry.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if signature is valid.|

**Example**
```
if (mz_zip_reader_entry_has_sign(zip_reader) == MZ_OK) {
    printf("Entry has signature attached\n");
    if (mz_zip_reader_entry_sign_verify(zip_reader) == MZ_OK) {
        printf("Entry signature is valid\n);
    } else {
        printf("Entry signature is invalid\n");
    }
}
```

### mz_zip_reader_entry_get_hash

Gets a hash algorithm from the entry's extra field.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint16_t|algorithm|[MZ_HASH](mz_hash.md) algorithm identifier|
|uint8_t *|digest|Digest buffer|
|int32_t|digest_size|Maximum digest buffer size|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if hash found.|

**Example**
```
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    uint8_t sha1_digest[MZ_HASH_SHA1_SIZE];
    if (mz_zip_reader_entry_get_hash(zip_reader, MZ_HASH_SHA1, sha1_digest, sizeof(sha1_digest)) == MZ_OK) {
        printf("Found sha1 digest for entry\n");
    }
}
```

### mz_zip_reader_entry_get_first_hash

Gets the most secure hash algorithm from the entry's extra field.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint16_t *|algorithm|Pointer to store [MZ_HASH](mz_hash.md) algorithm identifier|
|uint16_t *|digest_size|Pointer to store digest size|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if hash found.|

**Example**
```
uint16_t algorithm = 0;
uint16_t digest_size = 0;
if (mz_zip_reader_entry_get_first_hash(zip_reader, &algorithm, &digest_size) == MZ_OK) {
    printf("Found hash: algo %d size %d\n", algorithm, digest_size);
}
```

### mz_zip_reader_entry_get_info

Gets the current entry file info.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|mz_zip_file **|file_info|Pointer to [mz_zip_file](mz_zip_file.md) structure|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
mz_zip_file *file_info = NULL;
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    if (mz_zip_reader_entry_get_info(zip_reader, &file_info) == MZ_OK) {
        printf("First entry: %s\n", file_info->filename);
    }
}
```

### mz_zip_reader_entry_is_dir

Gets the current entry is a directory.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    if (mz_zip_reader_entry_is_dir(zip_reader) == MZ_OK) {
        printf("Entry is a directory\n");
    }
}
```

### mz_zip_reader_entry_save

Save the current entry to a steam. Each time the function needs to write to the stream it will call the _mz_stream_write_cb_ callback with the _stream_ pointer. This is a blocking call that will not return until the entire entry is written to the stream or until an error has occured.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|stream|_mz_stream_ instance|
|mz_stream_write_cb|write_cb|Stream write callback|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *file_stream = NULL;
const char *path = "c:\\my.zip";
const char *entry_path = "c:\\entry.dat";

mz_zip_reader_create(&zip_reader);

err = mz_zip_reader_open_file(zip_reader, path);
if (err == MZ_OK) {
    printf("Zip reader was opened %s\n", path);
    err = mz_zip_reader_goto_first_entry(zip_reader);
    if (err == MZ_OK) {
        mz_stream_os_create(&entry_stream);
        err = mz_stream_os_open(entry_stream, entry_path, MZ_OPEN_MODE_WRITE);
        if (err == MZ_OK) {
            err = mz_zip_reader_entry_save(zip_reader, file_stream, mz_stream_os_write);
            mz_stream_os_close(entry_stream);
        }
        mz_stream_os_delete(&entry_stream);
    }
    mz_zip_reader_close(zip_reader);
}

mz_zip_reader_delete(&zip_reader);
```

### mz_zip_reader_entry_save_process

Saves a portion of the current entry to a stream. Each time the function is called it will read from the zip file once and then write the output to the _mz_stream_write_cb_ callback with _stream_ pointer. This is intended to be used when writing zip file in a process loop.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|stream|_mz_stream_ instance|
|mz_stream_write_cb|write_cb|Stream write callback|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if more to process, MZ_END_OF_STREAM if no more data to process.|

**Example**
```
int32_t err = MZ_OK;
// TODO: Open zip reader and entry stream.
while (1) {
    err = mz_zip_reader_entry_save_process(zip_reader, entry_stream, mz_stream_os_write);
    if (err != MZ_OK) {
        printf("There was an error writing to stream (%d)\n", err);
        break;
    }
    if (err == MZ_END_OF_STREAM) {
        printf("Finished writing to stream\n");
        break;
    }
}
```

### mz_zip_reader_entry_save_file

Save the current entry to a file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|path|Path to save entry on disk|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    if (mz_zip_reader_entry_save_file(zip_reader, "entry1.bin") == MZ_OK) {
        printf("First entry saved to disk successfully\n");
    }
}
```

### mz_zip_reader_entry_save_buffer

Save the current entry to a memory buffer. To get the size required use _mz_zip_reader_entry_save_buffer_length_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|buf|Buffer to decompress to|
|int32_t|len|Maximum size of buffer|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful, or MZ_BUF_ERROR if _buf_ is too small.|

**Example**
```
int32_t buf_size = (int32_t)mz_zip_reader_entry_save_buffer_length(zip_reader);
char *buf = (char *)malloc(buf_size);
int32_t err = mz_zip_reader_entry_save_buffer(zip_reader, buf, buf_size);
if (err == MZ_OK) {
    // TODO: Do something with buffer
}
free(buf);
```

### mz_zip_reader_entry_save_buffer_length

Gets the length of the buffer required to save.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
int32_t buf_size = (int32_t)mz_zip_reader_entry_save_buffer_length(zip_reader);
char *buf = (char *)malloc(buf_size);
int32_t err = mz_zip_reader_entry_save_buffer(zip_reader, buf, buf_size);
if (err == MZ_OK) {
    // TODO: Do something with buffer
}
free(buf);
```

## Reader Bulk Extract

### mz_zip_reader_save_all

Save all files into a directory.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|destination_dir|Directory to extract all files to|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *destination_dir = "c:\\temp\\";
if (mz_zip_reader_save_all(zip_reader, destination_dir) == MZ_OK) {
    printf("All files successfully saved to %s\n", destination_dir);
}
```

## Reader Object

### mz_zip_reader_set_pattern

Sets the match pattern for entries in the zip file, if null all entries are matched. This match pattern is used when calling _mz_zip_reader_goto_first_entry_ and _mz_zip_reader_goto_next_entry_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|pattern|Search pattern or NULL if not used|
|uint8_t|ignore_case|Ignore case when matching if 1|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
int32_t matches = 0;
const char *pattern = "*.txt";
mz_zip_reader_set_pattern(zip_reader, pattern, 1);
if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK) {
    do {
        matches += 1;
    } while (mz_zip_reader_goto_next_entry(zip_reader) == MZ_OK);
}
printf("Found %d zip entries matching pattern %s\n", matches, pattern);
```

### mz_zip_reader_set_password

Sets the password required for extracting entire zip file. If not specified, then _mz_zip_reader_password_cb_ will be called for password protected zip entries.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|password|Password to use for entire zip file|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_reader_set_password(zip_handle, "mypassword");
```

### mz_zip_reader_set_raw

Sets whether or not it should save the entry raw.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint8_t|raw|Save entry as raw data if 1|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_reader_set_raw(zip_reader, 1);
```

### mz_zip_reader_get_raw

Gets whether or not it should save the entry raw.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint8_t *|raw|Pointer to store if saving as raw data|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
uint8_t raw = 0;
mz_zip_reader_get_raw(zip_reader, &raw);
printf("Entry will be saved as %s data\n", (raw) ? "raw gzip" : "decompressed");
```

### mz_zip_reader_get_zip_cd

Gets whether or not the archive has a zipped central directory.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint8_t *|zip_cd|Pointer to store if central directory is zipped|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
uint8_t zip_cd = 0;
mz_zip_reader_get_zip_cd(zip_reader, &zip_cd);
printf("Central directory %s zipped\n", (zip_cd) ? "is" : "is not");
```

### mz_zip_reader_get_comment

Gets the comment for the central directory.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char **|comment|Pointer to store global comment pointer|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *global_comment = NULL;
if (mz_zip_reader_get_comment(zip_reader, &global_comment) == MZ_OK) {
    printf("Zip comment: %s\n", global_comment);
}
```

### mz_zip_reader_set_recover

Sets the ability to recover the central dir by reading local file headers.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint8_t|recover|Set to 1 if recover method is supported, 0 otherwise.|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_reader_set_recover(zip_reader, 1);
```

### mz_zip_reader_set_encoding

Sets whether or not it should support a special character encoding in zip file names.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|int32_t|encoding|[MZ_ENCODING](mz_encoding.md) identifier|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_reader_set_encoding(zip_reader, MZ_ENCODING_CODEPAGE_437);
```

### mz_zip_reader_set_sign_required

Sets whether or not it a signature is required. If enabled, it will prevent extraction of zip entries that do not have verified signatures.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint8_t|sign_required|Valid CMS signatures are required if 1|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_reader_set_sign_required(zip_reader, 1);
```

### mz_zip_reader_set_overwrite_cb

Sets the callback for what to do when a file is about to be overwritten.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|User supplied data|
|mz_zip_reader_overwrite_cb|cb|_mz_zip_reader_overwrite_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_reader_overwrite_cb_.

### mz_zip_reader_set_password_cb

Sets the callback for what to do when a password is required and hasn't been set.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|User supplied data|
|mz_zip_reader_password_cb|cb|_mz_zip_reader_password_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_reader_password_cb_.

### mz_zip_reader_set_progress_cb

Sets the callback that gets called to update extraction progress. This callback is called on an interval specified by _mz_zip_reader_set_progress_interval_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|User supplied data|
|mz_zip_reader_progress_cb|cb|_mz_zip_reader_progress_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_reader_progress_cb_.

### mz_zip_reader_set_progress_interval

Let at least milliseconds pass between calls to progress callback.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|uint32_t|milliseconds|Number of milliseconds to wait before calling _mz_zip_reader_progress_cb_ during extraction|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_reader_set_progress_interval(zip_reader, 1000); // Wait 1 sec
```

### mz_zip_reader_set_entry_cb

Sets callback for when a new zip file entry is encountered during extraction.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void *|userdata|User supplied data|
|mz_zip_reader_entry_cb|cb|_mz_zip_reader_entry_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_reader_entry_cb_.

### mz_zip_reader_get_zip_handle

Gets the underlying zip instance handle.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|void **|zip_handle|Pointer to store _mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
void *zip_handle = NULL;
mz_zip_reader_get_zip_handle(zip_reader, &zip_handle);
mz_zip_goto_first_entry(zip_handle);
```

### mz_zip_reader_create

Creates a _mz_zip_reader_ instance and returns its pointer.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to store the _mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|void *|Pointer to the _mz_zip_reader_ instance|

**Example**
```
void *zip_reader = NULL;
mz_zip_reader_create(&zip_reader);
```

### mz_zip_reader_delete

Deletes a _mz_zip_reader_ instance and resets its pointer to zero.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
void *zip_reader = NULL;
mz_zip_reader_create(&zip_reader);
mz_zip_reader_delete(&zip_reader);
```

## Writer Callbacks

### mz_zip_writer_overwrite_cb

Callback that is called when it is about to overwrite an existing zip file. This callback can be set by calling _mz_zip_writer_set_overwrite_cb_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_writer_ instance|
|void *|userdata|User data pointer|
|const char *|path|Zip file path that will be overwritten|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if it should overwrite|

**Example**
```
int32_t writer_overwrite_cb(void *handle, void *userdata, const char *path) {
    printf("Zip file is going to be overwritten %s\n", path);
    // if not ok return MZ_INTERNAL_ERROR;
    return MZ_OK;
}
mz_zip_writer_set_overwrite_cb(zip_writer, NULL, writer_overwrite_cb);
```

Also see _minizip_add_overwrite_cb_ for advanced example.

### mz_zip_writer_password_cb

Callback that is called when it needs a password. Any entries that are added with the _MZ_ZIP_FLAG_ENCRYPTED_ flag will need a password. This callback can be set by calling _mz_zip_writer_set_password_cb_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_writer_ instance|
|void *|userdata|User data pointer|
|mz_zip_file *|file_info|Entry that needs password when adding|
|char *|password|Password character buffer|
|int32_t|max_password|Maximum password buffer size|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
int32_t writer_password_cb(void *handle, void *userdata, mz_zip_file *file_info, char *password, int32_t max_password) {
    printf("Supplying password for %s\n", file_info->filename);
    strncpy(password, "mypassword", max_password - 1);
    password[max_password - 1] = 0;
    return MZ_OK;
}
mz_zip_writer_set_password_cb(zip_writer, NULL, writer_password_cb);
```

### mz_zip_writer_progress_cb

Callback that is called during compression to report progress. It is called on an interval specified by _mz_zip_writer_set_progress_interval_. This callback can be set by calling _mz_zip_writer_set_progress_cb_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_writer_ instance|
|void *|userdata|User data pointer|
|mz_zip_file *|file_info|Entry that is being compressed|
|int64_t|position|File write position. To calculate progress when writing raw use `position / file_info->compressed_size`. To calculate progress when writing data to be compressed use `position / file_info->uncompressed_size`.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**

See example in _minizip_extract_progress_cb_.

### mz_zip_writer_entry_cb

Callback that is called for each entry that is compressed. This callback can be set by calling _mz_zip_writer_set_entry_cb.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_writer_ instance|
|void *|userdata|User data pointer|
|mz_zip_file *|file_info|Entry that is being compressed|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**

See example in _minizip_extract_entry_cb.

## Writer Open/Close

### mz_zip_writer_is_open

Checks to see if the zip file is open.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if open.|

**Example**
```
if (mz_zip_writer_is_open(zip_writer) == MZ_OK)
    printf("Zip file is open in writer\n");
```

### mz_zip_writer_open

 Opens zip file from stream.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|stream|_mz_stream_ instance|
|uint8_t|append|Opens in append mode if 1|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
void *file_stream = NULL;
const char *path = "c:\\my.zip";

mz_zip_writer_create(&zip_writer);
mz_stream_os_create(&file_stream);

err = mz_stream_os_open(file_stream, path, MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE);
if (err == MZ_OK) {
    err = mz_zip_writer_open(zip_writer, file_stream, 0);
    if (err == MZ_OK) {
        printf("Zip writer was opened %s\n", path);
        mz_zip_writer_close(zip_writer);
    }
}

mz_stream_os_delete(&file_stream);
mz_zip_writer_delete(&zip_writer);
```

### mz_zip_writer_open_file

Opens zip file from a file path.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_reader_ instance|
|const char *|path|Path to zip file|
|int64_t|disk_size|Disk size in bytes if using disk spanning, otherwise 0|
|uint8_t|append|Opens in append mode if 1|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
const char *path = "c:\\my.zip";
mz_zip_writer_create(&zip_writer);
if (mz_zip_writer_open_file(zip_writer, path, 0, 0) == MZ_OK) {
    printf("Zip writer was opened %s\n", path);
    mz_zip_writer_close(zip_writer);
}
mz_zip_writer_delete(&zip_writer);
```

### mz_zip_writer_open_file_in_memory

Opens zip file from a file path into memory for faster access.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|const char *|path|Path to zip file|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if opened.|

**Example**
```
const char *path = "c:\\my.zip";
mz_zip_writer_create(&zip_writer);
if (mz_zip_writer_open_file_in_memory(zip_writer, path) == MZ_OK) {
    printf("Zip writer was opened in memory %s\n", path);
    mz_zip_writer_close(zip_writer);
}
mz_zip_writer_delete(&zip_writer);
```

### mz_zip_writer_close

Closes the zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
if (mz_zip_writer_close(zip_writer) == MZ_OK)
    printf("Zip writer closed\n");
```

## Writer Entry

### mz_zip_writer_entry_open

Opens an entry in the zip file for writing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|mz_zip_file *|file_info|Zip entry info for entry that is being written|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
mz_zip_file file_info = { 0 };

file_info.filename = "newfile.txt";
file_info.modified_date = time(NULL);
file_info.version_madeby = MZ_VERSION_MADEBY;
file_info.compression_method = MZ_COMPRESS_METHOD_STORE;
file_info.flag = MZ_ZIP_FLAG_UTF8;

if (mz_zip_writer_entry_open(zip_writer, &file_info) == MZ_OK) {
    printf("Started writing new entry %s\n", file_info.filename);
}
```

### mz_zip_writer_entry_close

Closes entry in zip file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
if (mz_zip_writer_close(zip_writer) == MZ_OK) {
    printf("Stopped writing new entry\n");
}
```

### mz_zip_writer_entry_write

Writes data into entry for zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|buf|Buffer of data to write|
int32_t|len|Number of bytes to write|

**Return**
|Type|Description|
|-|-|
|int32_t|Bytes written or [MZ_ERROR](mz_error.md) code if less than 0.|


**Example**
```
mz_zip_file file_info = { 0 };

file_info.filename = "newfile.txt";
file_info.modified_date = time(NULL);
file_info.version_madeby = MZ_VERSION_MADEBY;
file_info.compression_method = MZ_COMPRESS_METHOD_STORE;
file_info.flag = MZ_ZIP_FLAG_UTF8;

if (mz_zip_writer_entry_open(zip_writer, &file_info) == MZ_OK) {
    printf("Started writing new entry %s\n", file_info.filename);
    int32_t bytes_written = mz_zip_writer_entry_write(zip_writer, "test", 4);
    if (bytes_written == 4) {
        printf("Successfully wrote test\n");
    }
    mz_zip_writer_entry_close(zip_writer);
}
```

## Writer Add/Compress

### mz_zip_writer_add

Writes all data to the currently open entry in the zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|stream|_mz_stream_ instance|
|mz_stream_read_cb|read_cb|Callback to read from when adding new entry|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
mz_stream_mem_create(&mem_stream);
mz_stream_mem_set_grow(mem_stream, 1);
mz_stream_mem_write(mem_stream, "test", 4);
mz_stream_mem_seek(mem_stream, 0, MZ_SEEK_SET);

mz_zip_file file_info = { 0 };

file_info.filename = "newfile.txt";
file_info.modified_date = time(NULL);
file_info.version_madeby = MZ_VERSION_MADEBY;
file_info.compression_method = MZ_COMPRESS_METHOD_STORE;
file_info.flag = MZ_ZIP_FLAG_UTF8;

if (mz_zip_writer_entry_open(zip_writer, &file_info) == MZ_OK) {
    if (mz_zip_writer_add(zip_writer, mem_stream, mz_stream_mem_read) == MZ_OK) {
        printf("Added new entry from stream\n");
    }
    mz_zip_writer_entry_close(zip_writer);
}

mz_stream_mem_delete(&mem_stream);
```

### mz_zip_writer_add_process

Writes a portion of data to the currently open entry in the zip. This function is intended to be used in process loops where you don't want to compress the entire file in one function.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|stream|_mz_stream_ instance|
|mz_stream_read_cb|read_cb|Callback to read from when adding new entry|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful or MZ_END_OF_STREAM if done.|

**Example**

See source code for _mz_zip_writer_add_.

### mz_zip_writer_add_info

Adds an entry to the zip based on the info.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|stream|_mz_stream_ instance|
|mz_stream_read_cb|read_cb|Callback to read from when adding new entry|
|mz_file_info *|file_info|Zip entry information for adding new entry|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
mz_stream_mem_create(&mem_stream);
mz_stream_mem_set_grow(mem_stream, 1);
mz_stream_mem_write(mem_stream, "test", 4);
mz_stream_mem_seek(mem_stream, 0, MZ_SEEK_SET);

mz_zip_file file_info = { 0 };

file_info.filename = "newfile.txt";
file_info.modified_date = time(NULL);
file_info.version_madeby = MZ_VERSION_MADEBY;
file_info.compression_method = MZ_COMPRESS_METHOD_STORE;
file_info.flag = MZ_ZIP_FLAG_UTF8;

if (mz_zip_writer_add_info(zip_writer, mem_stream, mz_stream_mem_read, &file_info) == MZ_OK) {
    printf("Added new entry from stream\n");
}

mz_stream_mem_delete(&mem_stream);
```

### mz_zip_writer_add_buffer

Adds an entry to the zip with a memory buffer.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|buf|Buffer to read when compressing|
|int32_t|len|Length of buffer to read|
|mz_file_info *|file_info|Zip entry information for adding new entry|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
mz_zip_file file_info = { 0 };

file_info.filename = "newfile.txt";
file_info.modified_date = time(NULL);
file_info.version_madeby = MZ_VERSION_MADEBY;
file_info.compression_method = MZ_COMPRESS_METHOD_STORE;
file_info.flag = MZ_ZIP_FLAG_UTF8;

char *contents = "test";

if (mz_zip_writer_add_buffer(zip_writer, contents, strlen(contents), &file_info) == MZ_OK) {
    printf("Added new entry from buffer\n");
}
```

### mz_zip_writer_add_file

Adds an entry to the zip from a file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|const char *|path|Path to file on disk to add|
|const char *|filename_in_zip|Filename in zip to write|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
const char *path = "c:\\mydatafiles\\tx101.txt";
const char *filename_in_zip = "101.txt;
if (mz_zip_writer_add_file(zip_writer, path, filename_in_zip) == MZ_OK) {
    printf("Entry added to zip %s as %s\n", path, filename_in_zip);
}
```

### mz_zip_writer_add_path

Enumerates a directory or pattern and adds entries to the zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|const char *|path|Path of directory to add (can include search pattern)|
|const char *|root_path|Root directory to start adding from. Entries will be named in the relative to this root path|
|uint8_t|include_path|Include the full path if 1|
|uint8_t|recursive|Process directory recursively if 1|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**
```
if (mz_zip_writer_add_path(zip_writer, "c:\\dir1\\dir2\\", "c:\\dir1\", 0, 1) == MZ_OK) {
    printf("Added entries from c:\\dir1\\dir2\\ recursively\n");
}
```

### mz_zip_writer_copy_from_reader

Adds an entry from a zip reader instance. This copies the current entry from the zip reader instance.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|reader|_mz_zip_reader_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful.|

**Example**

See source code for _minizip_erase_ where it erases a zip entry by copying all entries from the source zip file to the target zip file.

## Writer Object

### mz_zip_writer_set_password

Password to use for encrypting files in the zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|const  char *|password|Password string|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_password(zip_writer, "myzippass");
```

### mz_zip_writer_set_comment

Comment to use for the archive.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|const char *|comment|Global zip file comment string|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_comment(zip_writer, "This is my zip file -- hands off!");
```

### mz_zip_writer_set_raw

Sets whether or not we should write the entry raw.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|uint8_t|raw|Write data in zip in raw mode (don't compress) if 1|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_raw(zip_writer, 1);
```

### mz_zip_writer_get_raw

Gets whether or not we should write the entry raw.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|uint8_t *|raw|Pointer to store if using raw mode|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
uint8_t raw = 0;
mz_zip_writer_get_raw(zip_writer, &raw);
printf("Writing zip entries in mode: %s\n", (raw) ? "raw" : "normal");
```

### mz_zip_writer_set_aes

Use aes encryption when adding files in zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|uint8_t|aes|Encrypt files with AES 256-bit if 1|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_aes(zip_writer, 1);
```

### mz_zip_writer_set_compress_method

Sets the compression method when adding files in zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|uint16_t|compress_method|[MZ_COMPRESS_METHOD](mz_compress_method.md) compression method when adding entries|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_compress_method(zip_writer, MZ_COMPRESS_METHOD_STORE);
```

### mz_zip_writer_set_compress_level

Sets the compression level when adding files in zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|int16_t|compress_level|[MZ_COMPRESS_LEVEL](mz_compress_level.md) compression level when adding entries|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_compress_level(zip_writer, MZ_COMPRESS_LEVEL_BEST);
```

### mz_zip_writer_set_zip_cd

Sets whether or not the central directory should be zipped.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|uint8_t|zip_cd|Zip the central directory if 1|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_zip_cd(zip_writer, 1);
```

### mz_zip_writer_set_certificate

Sets the certificate and timestamp url to use for signing when adding files in zip.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|const char *|cert_path|Path to certificate to sign entries with|
|const char *|cert_pwd|Password for certificate to sign with|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
mz_zip_writer_set_certificate(zip_writer, "c:\\mycerts\\zip_cert.pfx", "mycertpwd");
```

### mz_zip_writer_set_overwrite_cb

Sets the callback for what to do when a zip file is about to be overwritten.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|userdata|User supplied data|
|mz_zip_writer_overwrite_cb|cb|_mz_zip_writer_overwrite_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_writer_overwrite_cb_.

### mz_zip_writer_set_password_cb

Sets the callback for what to do when a password for an entry.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|userdata|User supplied data|
|mz_zip_writer_password_cb|cb|_mz_zip_writer_password_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_writer_password_cb_.

### mz_zip_writer_set_progress_cb

Sets the callback that gets called to update compression progress. This callback is called on an interval specified by _mz_zip_writer_set_progress_interval_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|userdata|User supplied data|
|mz_zip_writer_progress_cb|cb|_mz_zip_writer_progress_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_writer_progress_cb_.

### mz_zip_writer_set_progress_interval

Let at least milliseconds pass between calls to progress callback.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|uint32_t|milliseconds|Number of milliseconds to wait before calling _mz_zip_writer_progress_cb_ during extraction|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
mz_zip_writer_set_progress_interval(zip_writer, 1000); // Wait 1 sec
```

### mz_zip_writer_set_entry_cb

Sets callback for when a new zip file entry is encountered during compression.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void *|userdata|User supplied data|
|mz_zip_writer_entry_cb|cb|_mz_zip_writer_entry_cb_ function pointer|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**

See example for _mz_zip_writer_entry_cb_.

### mz_zip_writer_get_zip_handle

Gets the underlying zip instance handle.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void *|handle|_mz_zip_writer_ instance|
|void **|zip_handle|Pointer to store _mz_zip_ instance|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
void *zip_handle = NULL;
mz_zip_writer_get_zip_handle(zip_writer, &zip_handle);
```

### mz_zip_writer_create

Creates a _mz_zip_writer_ instance and returns its pointer.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to store the _mz_zip_writer_ instance|

**Return**
|Type|Description|
|-|-|
|void *|Pointer to the _mz_zip_writer_ instance|

**Example**
```
void *zip_writer = NULL;
mz_zip_writer_create(&zip_writer);
```

### mz_zip_writer_delete

Deletes a _mz_zip_writer_ instance and resets its pointer to zero.

**Arguments**
|Type|Name|Description|
|-|-|-|
|void **|handle|Pointer to the _mz_zip_writer_ instance|

**Return**
|Type|Description|
|-|-|
|void|No return|

**Example**
```
void *zip_writer = NULL;
mz_zip_writer_create(&zip_writer);
mz_zip_writer_delete(&zip_writer);
```
