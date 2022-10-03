# MZ_ZIP_FILE

Zip entry information structure. The _mz_zip_file_ structure is populated when reading zip entry information and can be used to populate zip entry information when writing zip entries.

|Type|Name|Description|[PKWARE zip app note](zip/appnote.txt) section|
|-|-|-|-|
|uint16_t|version_madeby|Version made by field|4.4.2|
|uint16_t|version_needed|Version needed to extract|4.4.3|
|uint16_t|flag|General purpose bit flag|4.4.4|
|uint16_t|compression_method|Compression method|4.4.5 [MZ_COMPRESS_METHOD](mz_compress_method.md)|
|time_t|modified_date|Last modified unix timestamp|4.4.6, 4.5.5, 4.5.7|
|time_t|accessed_date|Last accessed unix timestamp|4.5.5, 4.5.7|
|time_t|creation_date|Creation date unix timestamp|4.5.5|
|uint32_t|crc|CRC32-B hash of uncompressed data|4.4.7|
|int64_t|compressed_size|Compressed size|4.4.8|
|int64_t|uncompressed_size|Uncompressed size|4.4.9|
|uint16_t|filename_size|Filename length|4.4.10|
|uint16_t|extrafield_size|Extrafield length|4.4.11|
|uint16_t|comment_size|Comment size|4.4.12|
|uint32_t|disk_number|Starting disk number|4.4.13|
|int64_t|disk_offset|Starting disk offset|4.4.16|
|uint16_t|internal_fa|Internal file attributes|4.4.14|
|uint16_t|external_fa|External file attributes|4.4.15|
|const char *|filename|Filename UTF-8 null-terminated string|4.4.17|
|const uint8_t *|extrafield|Extrafield buffer array|4.4.28|
|const char *|comment|Comment UTF-8 null-terminated string|4.4.18|
|uint16_t|zip64|Zip64 extension mode|[MZ_ZIP64](mz_zip64.md)|
|uint16_t|aes_version|WinZip AES version|[WinZip AES App Note](zip/winzip_aes.md)|
|uint16_t|aes_encryption_mode|WinZip AES encryption mode|[WinZip AES App Note](zip/winzip_aes.md)|

For more information about each field please consult the referenced app note section.

## Extended Notes

### verison_madeby

> The upper byte indicates the compatibility of the file attribute information... The lower byte indicates the ZIP specification version... supported by the software used to encode the file.

The preprocessor define `MZ_VERSION_MADEBY` contains the version made by value for the current compiler runtime. To get the file attribute information use the preprocessor define `MZ_HOST_SYSTEM(version_madeby)`.

### version_needed

When writing zip entries, this will automatically be filled in if the value is zero.

### flag

|Flag|Value|Description|
|-|-|-|
| MZ_ZIP_FLAG_ENCRYPTED | 0x1 | Entry is encrypted. If using AES encryption `aes_version` needs to be set to `MZ_AES_VERSION` |
| MZ_ZIP_FLAG_LZMA_EOS_MARKER | 0x2 | Entry contains LZMA end of stream marker |
| MZ_ZIP_FLAG_DEFLATE_MAX | 0x2 | Entry compressed with deflate max algorithm |
| MZ_ZIP_FLAG_DEFLATE_NORMAL | 0 | Entry compressed with deflate normal algorithm |
| MZ_ZIP_FLAG_DEFLATE_FAST | 0x4 | Entry compressed with deflate fast algorithm |
| MZ_ZIP_FLAG_DEFLATE_SUPER_FAST | MAX + FAST | Entry compressed with deflate super fast algorithm |
| MZ_ZIP_FLAG_DATA_DESCRIPTOR | 0x08 | Entry contains data descriptor bytes at the end of the compressed content which contain the compressed and uncompressed size. Local file header contains zeros for these values. |
| MZ_ZIP_FLAG_UTF8 | 0x800 | Entry filename is UTF-8 encoded |
| MZ_ZIP_FLAG_MASK_LOCAL_INFO | 0x2000 | Local file header info is masked |

### creation_date

Creation date is only supported on Windows.

### external_fa

External file attributes. These attributes are native host system attribute values for the entry. To get the host system use `MZ_HOST_SYSTEM(version_madeby)`. It is possible to convert from one host system's attributes to another using `mz_zip_attrib_convert`.

### aes_version

This attribute must be set to `MZ_AES_VERSION` when AES encryption is used.

### aes_encryption_mode

AES encryption mode, by default 256-bit encryption is used for compression.

|Flag|Value|Description|
|-|-|-|
| MZ_AES_ENCRYPTION_MODE_128 | 0x01 | 128-bit AES encryption |
| MZ_AES_ENCRYPTION_MODE_192 | 0x02 | 192-bit AES encryption |
| MZ_AES_ENCRYPTION_MODE_256 | 0x03 | 256-bit AES encryption |