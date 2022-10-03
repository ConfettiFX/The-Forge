# MZ_OS <!-- omit in toc -->

These functions provide support for handling common file system operations.

- [Path](#path)
  - [mz_path_combine](#mz_path_combine)
  - [mz_path_append_slash](#mz_path_append_slash)
  - [mz_path_remove_slash](#mz_path_remove_slash)
  - [mz_path_has_slash](#mz_path_has_slash)
  - [mz_path_convert_slashes](#mz_path_convert_slashes)
  - [mz_path_compare_wc](#mz_path_compare_wc)
  - [mz_path_resolve](#mz_path_resolve)
  - [mz_path_remove_filename](#mz_path_remove_filename)
  - [mz_path_remove_extension](#mz_path_remove_extension)
  - [mz_path_get_filename](#mz_path_get_filename)
- [Directory](#directory)
  - [mz_dir_make](#mz_dir_make)
- [File](#file)
  - [mz_file_get_crc](#mz_file_get_crc)
- [Operating System](#operating-system)
  - [mz_os_unicode_string_create](#mz_os_unicode_string_create)
  - [mz_os_unicode_string_delete](#mz_os_unicode_string_delete)
  - [mz_os_utf8_string_create](#mz_os_utf8_string_create)
  - [mz_os_utf8_string_delete](#mz_os_utf8_string_delete)
  - [mz_os_rand](#mz_os_rand)
  - [mz_os_rename](#mz_os_rename)
  - [mz_os_unlink](#mz_os_unlink)
  - [mz_os_file_exists](#mz_os_file_exists)
  - [mz_os_get_file_size](#mz_os_get_file_size)
  - [mz_os_get_file_date](#mz_os_get_file_date)
  - [mz_os_set_file_date](#mz_os_set_file_date)
  - [mz_os_get_file_attribs](#mz_os_get_file_attribs)
  - [mz_os_set_file_attribs](#mz_os_set_file_attribs)
  - [mz_os_make_dir](#mz_os_make_dir)
  - [mz_os_open_dir](#mz_os_open_dir)
  - [mz_os_read_dir](#mz_os_read_dir)
  - [mz_os_close_dir](#mz_os_close_dir)
  - [mz_os_is_dir](#mz_os_is_dir)
  - [mz_os_is_symlink](#mz_os_is_symlink)
  - [mz_os_make_symlink](#mz_os_make_symlink)
  - [mz_os_read_symlink](#mz_os_read_symlink)
  - [mz_os_ms_time](#mz_os_ms_time)

## Path

The _mz_path_ family of functions are helper functions used when constructing file system paths.

### mz_path_combine

Combines two paths.

**Arguments**
|Type|Name|Description|
|-|-|-|
|char *|path|Base path|
|const char *|join|Path to append|
|int32_t|max_path|Maximum path buffer size|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
char path[120];
strncat(path, "c:\\windows\\", sizeof(path));
mz_path_combine(path, "temp", sizeof(path));
printf("Combined path: %s\n", path);
```

### mz_path_append_slash

Appends a path slash on to the end of the path. To get the path slash character for the compiler platform use _MZ_PATH_SLASH_PLATFORM_ preprocessor define.

**Arguments**
|Type|Name|Description|
|-|-|-|
|char *|path|Path|
|int32_t|max_path|Maximum bytes to store path|
|char|slash|Path slash character|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
char path[120];
strncat(path, "c:\\windows", sizeof(path));
mz_path_append_slash(path, sizeof(path), MZ_PATH_SLASH_PLATFORM);
printf("Path with end slash: %s\n", path);
```

### mz_path_remove_slash

Removes a path slash from the end of the path.

**Arguments**
|Type|Name|Description|
|-|-|-|
|char *|path|Path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
char path[120];
strncat(path, "c:\\windows\\", sizeof(path));
mz_path_remove_slash(path);
printf("Path with no slash: %s\n", path);
```

### mz_path_has_slash

Returns whether or not the path ends with slash.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if has slash.|

**Example**
```
const char *path = "c:\\windows\\";
if (mz_path_has_slash(path) == MZ_OK)
    printf("Path ends with a slash\n");
else
    printf("Path does not end with a slash\n");
```

### mz_path_convert_slashes

Converts the slashes in a path. This can be used to convert all unix path slashes to windows path slashes or conver all windows path slashes to unix path slashes. If there are mixed slashes in the path, it can unify them to all one format.

**Arguments**
|Type|Name|Description|
|-|-|-|
|char *|path|Path|
|char|slash|Path slash character|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if has slash.|

**Example**
```
char path[120];
strncat(path, "c:\\windows\\", sizeof(path));
if (mz_path_convert_slashes(path, MZ_PATH_SLASH_UNIX) == MZ_OK)
    printf("Path converted to unix slashes: %s\n", path);
```

### mz_path_compare_wc

Compares two paths with a wildcard.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Path|
|const char *|wildcard|Wildcard pattern|
|uint8_t|ignore_case|Ignore case during comparison if 1.|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if matched, MZ_EXIST_ERROR if not matched.|

**Example**
```
const char *path = "test.txt";
if (mz_path_compare_wc(path, "*.txt", 0) == MZ_OK)
    printf("%s is a text file\n", path);
else
    printf("%s is not a text file\n", path);
```

### mz_path_resolve

Resolves a path. Path parts that only contain dots will be resolved. If a path part contains a single dot, it will be remoed. If a path part contains two dots, it will remove the last path part. This function can be used to prevent the  _zipslip_ vulnerability and ensure that files are not written outside of their intended target.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Path|
|char *|target|Resolved path character array|
|int32_t|max_target|Maximum size of resolved path array|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\windows\\..\\";
char target[120];
mz_path_resolve(path, target, sizeof(target));
printf("Resolved path: %s\n", target);
```

### mz_path_remove_filename

Removes the filename from a path.

**Arguments**
|Type|Name|Description|
|-|-|-|
|char *|path|Path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
char path[120];
strncpy(path, "c:\\windows\\test.txt", sizeof(path));
printf("Path: %s\n", path);
mz_path_remove_filename(path);
printf("Path with no filename: %s\n", path);
```

### mz_path_remove_extension

Remove the extension from a path.

**Arguments**
|Type|Name|Description|
|-|-|-|
|char *|path|Path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
char path[120];
strncpy(path, "c:\\windows\\test.txt", sizeof(path));
printf("Path: %s\n", path);
mz_path_remove_extension(path);
printf("Path with no file extension: %s\n", path);
```

### mz_path_get_filename

Get the filename from a path.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Path|
|const char **|filename|Pointer to filename string|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\windows\\test.txt";
const char *filename = NULL;
printf("Path: %s\n", path);
if (mz_path_get_filename(path, &filename) == MZ_OK)
    printf("Filename: %s\n", filename);
else
    printf("Path has no filename\n");
```

## Directory

### mz_dir_make

Creates a directory recursively.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\temp\\x\\y\\z\\";
if (mz_dir_make(path) == MZ_OK)
    printf("Dir was created: %s\n", path);
else
    printf("Dir was not created: %s\n", path);
```

## File

### mz_file_get_crc

Gets the crc32 hash of a file. This function helps provide functional backwards compatibility.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Path to calculate CRC value for|
|uint32_t *|result_crc|Pointer to store the calculated CRC value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\temp\\test.txt";
uint32_t crc = 0;
if (mz_file_get_crc(path, &crc) == MZ_OK)
    printf("File %s CRC: %08x\n", path, crc);
else
    printf("Failed to calculate CRC: %s\n", path);
```

## Operating System

The _mz_os_ family of functions wrap all platform specific code necessary to zip and unzip files.

### mz_os_unicode_string_create

Create unicode string from a string with another encoding.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|string|String to convert|
|int32_t|encoding|String encoding (See [MZ_ENCODING](mz_encoding.md))|

**Return**
|Type|Description|
|-|-|
|wchar_t *|Returns pointer to unicode string if successful, otherwise NULL.|

**Example**
```
char *test = "test";
wchar_t *test_unicode = mz_os_unicode_string_create(test, MZ_ENCODING_UTF8);
if (test_unicode != NULL) {
    printf("Unicode test string created\n");
    mz_os_unicode_string_delete(&test_unicode);
}
```

### mz_os_unicode_string_delete

Delete a unicode string that was created with _mz_os_unicode_string_create_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|wchar_t **|string|Pointer to unicode string|

**Example**
```
char *test = "test";
wchar_t *test_unicode = mz_os_unicode_string_create(test, MZ_ENCODING_UTF8);
if (test_unicode != NULL) {
    printf("Unicode test string created\n");
    mz_os_unicode_string_delete(&test_unicode);
}
```

### mz_os_utf8_string_create

Create a utf8 string from a string with another encoding.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|string|String to convert|
|int32_t|encoding|String encoding (See [MZ_ENCODING](mz_encoding.md))|

**Return**
|Type|Description|
|-|-|
|uint8_t *|Returns pointer to UTF-8 encoded string if successful, otherwise NULL.|

**Example**
```
char *test = "test";
wchar_t *test_utf8 = mz_os_utf8_string_create(test, MZ_ENCODING_CODEPAGE_437);
if (test_utf8 != NULL) {
    printf("UTF-8 test string created\n");
    mz_os_utf8_string_create(&test_utf8);
}
```

### mz_os_utf8_string_delete

Delete a utf8 string that was created with _mz_os_utf8_string_create_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint8_t **|string|Pointer to utf8 encoded string|

**Example**
```
char *test = "test";
wchar_t *test_utf8 = mz_os_utf8_string_create(test, MZ_ENCODING_CODEPAGE_437);
if (test_utf8 != NULL) {
    printf("UTF-8 test string created\n");
    mz_os_utf8_string_create(&test_utf8);
}
```

### mz_os_rand

Random number generator (not cryptographically secure). For a cryptographically secure random number generator use _mz_crypt_rand_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|uint8_t *|buf|Buffer to fill with random data|
|int32_t|size|Maximum size of buffer array|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
uint8_t buf[120];
if (mz_os_rand(buf, sizeof(buf)) == MZ_OK)
    printf("%d bytes of random data generated\n", sizeof(buf));
```

### mz_os_rename

Rename a file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|source_path|Original path|
|const char *|target_path|New path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_os_rename("c:\\test1.txt", "c:\\test2.txt") == MZ_OK)
    printf("File was renamed successfully\n");
```

### mz_os_unlink

Delete an existing file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_os_unlink("c:\\test2.txt") == MZ_OK)
    printf("File was deleted successfully\n");
```

### mz_os_file_exists

Check to see if a file exists.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\test2.txt";
if (mz_os_file_exists(path) == MZ_OK)
    printf("File %s exists\n", path);
else
    printf("File %s does not exist\n", path);
```

### mz_os_get_file_size

Gets the length of a file.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|

**Return**
|Type|Description|
|-|-|
|int64_t|Size of file, does not check for existence of file.|

**Example**
```
const char *path = "c:\\test3.txt";
if (mz_os_file_exists(path) == MZ_OK) {
    int64_t file_size = mz_os_get_file_size(path);
    printf("File %s size %lld\n", path, file_size);
} else {
    printf("File %s does not exist\n", path);
}
```

### mz_os_get_file_date

Gets a file's modified, access, and creation dates if supported. Creation date is not supported on Linux based systems and zero is returned for _creation_date_ on those systems.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|
|time_t *|modified_date|Pointer to store file's modified unix timestamp|
|time_t *|accessed_date|Pointer to store file's accessed unix timestamp|
|time_t *|creation_date|Pointer to store file's creation unix timestamp|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\test4.txt";
time_t modified_date, accessed_date, creation_date;
if (mz_os_get_file_date(path, &modified_date, &accessed_date, &creation_date) == MZ_OK)
    printf("File %s modified %lld accessed %lld creation %lld\n", path, modified_date, accessed_date, creation_date);
```

### mz_os_set_file_date

Sets a file's modified, access, and creation dates if supported. Creation date is not supported on Linux based systems.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|
|time_t|modified_date|File's modified unix timestamp|
|time_t|accessed_date|File's accessed unix timestamp|
|time_t|creation_date|File's creation unix timestamp|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *src_path = "c:\\test4.txt";
const char *target_path = "c:\\test5.txt";
time_t modified_date, accessed_date, creation_date;
if (mz_os_get_file_date(src_path, &modified_date, &accessed_date, &creation_date) == MZ_OK) {
    printf("Source file %s modified %lld accessed %lld creation %lld\n", path, modified_date, accessed_date, creation_date);
    if (mz_os_set_file_date(target_path, modified_date, accessed_date, creation_date) == MZ_OK) {
        printf("Target file dates changed successfully\n");
    }
}
```

### mz_os_get_file_attribs

Gets a file's attributes.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|
|uint32_t *|attributes|Pointer to store file attributes value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\test6.txt";
uint32_t attributes = 0;
if (mz_os_get_file_attribs(path, &attributes) == MZ_OK)
    printf("File %s attributes %08x\n", attributes);
```

### mz_os_set_file_attribs

Sets a file's attributes.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File path|
|uint32_t|attributes|File attributes value|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\test6.txt";
uint32_t attributes = 0;
if (mz_os_get_file_attribs(path, &attributes) == MZ_OK) {
    printf("File %s attributes %08x\n", attributes);
    attributes |= FILE_ATTRIBUTE_READONLY;
    if (mz_os_set_file_attribs(path, attributes) == MZ_OK) {
        printf("File changed to readonly\n");
    }
}
```

### mz_os_make_dir

Creates a directory. To recursively create a directory use _mz_dir_make_.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Directory path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
if (mz_os_make_dir("c:\\testdir\\") == MZ_OK)
    printf("Test directory created\n");
```

### mz_os_open_dir

Opens a directory for listing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Directory path|

**Return**
|Type|Description|
|-|-|
|DIR *|Directory enumeration handle, returns NULL if error.|

**Example**
```
const char *search_dir = "c:\\test1\\";
DIR *dir = mz_open_dir(search_dir);
if (dir != NULL) {
    printf("Dir %s was opened\n", search_dir);
    mz_os_close_dir(dir);
}
```

### mz_os_read_dir

Reads a directory listing entry.

**Arguments**
|Type|Name|Description|
|-|-|-|
|DIR *|dir|Directory enumeration handle|

**Return**
|Type|Description|
|-|-|
|struct dirent *|Pointer to directory entry information structure. To get the name of the directory use the _d_name_ structure field.|

**Example**
```
const char *search_dir = "c:\\test2\\";
DIR *dir = mz_open_dir(search_dir);
if (dir != NULL) {
    struct dirent *entry = NULL;
    printf("Dir %s was opened\n", search_dir);
    while ((entry = mz_os_read_dir(dir)) != NULL) {
        printf("Dir entry: %s was opened\n", entry->d_name);
    }
    mz_os_close_dir(dir);
}
```

### mz_os_close_dir

Closes a directory that has been opened for listing.

**Arguments**
|Type|Name|Description|
|-|-|-|
|DIR *|dir|Directory enumeration handle|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *search_dir = "c:\\test3\\";
DIR *dir = mz_open_dir(search_dir);
if (dir != NULL) {
    struct dirent *entry = NULL;
    printf("Dir %s was opened\n", search_dir);
    while ((entry = mz_os_read_dir(dir)) != NULL) {
        printf("Dir entry: %s was opened\n", entry->d_name);
    }
    mz_os_close_dir(dir);
}
```

### mz_os_is_dir

Checks to see if path is a directory.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File system path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if it is a directory.|

**Example**
```
const char *path = "c:\\test7.txt";
if (mz_os_is_dir(path) == MZ_OK)
    printf("Path %s is a directory\n", path);
else
    printf("Path %s is not a directory\n", path);
```

### mz_os_is_symlink

Checks to see if path is a symbolic link.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|File system path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if it is a symbolic link.|

**Example**
```
const char *path = "c:\\test7.txt";
if (mz_os_is_symlink(path) == MZ_OK)
    printf("Path %s is a symbolic link\n", path);
else
    printf("Path %s is not a symbolic link\n", path);
```

### mz_os_make_symlink

Creates a symbolic link pointing to a target.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Link path|
|const char *|target_path|Actual path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\test7.txt";
const char *target_path = "c:\\test8.txt";
if (mz_os_make_symlink(path, target_path) == MZ_OK)
    printf("Symbolic link created at %s pointing to %s\n", path, target_path);
```

### mz_os_read_symlink

Gets the target path for a symbolic link.

**Arguments**
|Type|Name|Description|
|-|-|-|
|const char *|path|Link path|
|char *|target_path|Actual path|
|int32_t|max_path|Maximum bytes to store actual path|

**Return**
|Type|Description|
|-|-|
|int32_t|[MZ_ERROR](mz_error.md) code, MZ_OK if successful|

**Example**
```
const char *path = "c:\\test7.txt";
const char *target_path = "c:\\test8.txt";
if (mz_os_make_symlink(path, target_path) == MZ_OK) {
    char actual_path[120];
    printf("Symbolic link created at %s pointing to %s\n", path, target_path);
    if (mz_os_read_symlink(path, actual_path, sizeof(actual_path)) == MZ_OK) {
        if (strcmp(target_path, actual_path) == 0) {
            printf("Confirmed symbolic link created\n");
        }
    }
}
```

### mz_os_ms_time

Gets the time in milliseconds.

**Return**
|Type|Description|
|-|-|
|uint64_t|Current time in milliseconds|

**Example**
```
uint64_t current_time = mz_os_ms_time();
printf("Current time in %lldms\n", current_time);
```
