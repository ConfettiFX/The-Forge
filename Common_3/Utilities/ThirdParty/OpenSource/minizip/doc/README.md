# minizip-ng Documentation  <!-- omit in toc -->

### Table of Contents

- [API](#api)
- [Limitations](#limitations)
- [Xcode Instructions](#xcode-instructions)
- [Zlib Configuration](#zlib-configuration)
- [Upgrading from 1.x](#upgrading-from-1x)
- [Security Considerations](#security-considerations)

## API

### Constants <!-- omit in toc -->

|Prefix|Description|
|-|-|
|[MZ_COMPRESS_LEVEL](mz_compress_level.md)|Compression level enumeration|
|[MZ_COMPRESS_METHOD](mz_compress_method.md)|Compression method enumeration|
|[MZ_ENCODING](mz_encoding.md)|Character encoding enumeration|
|[MZ_ERROR](mz_error.md)|Error constants|
|[MZ_HASH](mz_hash.md)|Hash algorithms and digest sizes
|[MZ_HOST_SYSTEM](mz_host_system.md)|System identifiers|
|[MZ_OPEN_MODE](mz_open_mode.md)|Stream open modes|
|[MZ_SEEK](mz_seek.md)|Stream seek origins|
|[MZ_ZIP64](mz_zip64.md)|Zip64 extrafield options|

### Interfaces <!-- omit in toc -->

|Name|Description|
|-|-|
|MZ_COMPAT|Old minizip 1.x compatibility layer|
|[MZ_OS](mz_os.md)|Operating system level file system operations|
|[MZ_ZIP](mz_zip.md)|Zip archive and entry interface |
|[MZ_ZIP_RW](mz_zip_rw.md)|Easy zip file extraction and creation|

### Structures <!-- omit in toc -->

|Name|Description|
|-|-|
|[MZ_ZIP_FILE](mz_zip_file.md)|Zip entry information|

### Extrafield Proposals <!-- omit in toc -->

The zip reader and writer interface provides support for extended hash algorithms for zip entries, compression of the central directory, and the adding and verifying of CMS signatures for each entry. In order to add support for these features, extrafields were added and are described in the [minizip extrafield documentation](mz_extrafield.md).

## Limitations

+ Archives are required to have a central directory unless recovery mode is enabled.
+ Central directory header values should be correct and it is necessary for the compressed size to be accurate for encryption.
+ Central directory is the only data stored on the last disk of a split-disk archive and doesn't follow disk size restrictions.

### Third-Party Limitations <!-- omit in toc -->

* Windows Explorer zip extraction utility does not support disk splitting. [1](https://stackoverflow.com/questions/31286707/the-same-volume-can-not-be-used-as-both-the-source-and-destination)
* macOS archive utility does not properly support ZIP files over 4GB. [1](http://web.archive.org/web/20140331005235/http://www.springyarchiver.com/blog/topic/topic/203) [2](https://bitinn.net/10716/)

## Xcode Instructions

To create an Xcode project with CMake use:
```
cmake -G Xcode .
```

## Zlib Configuration

By default, if zlib is not found, it will be pulled as an external project and installed. This requires [Git](https://git-scm.com/) to be installed and available to your command interpreter.

* To specify your own zlib repository use `ZLIB_REPOSITORY` and/or `ZLIB_TAG`.
* To specify your own zlib installation use `ZLIB_LIBRARY` and `ZLIB_INCLUDE_DIR`.

**Compiling with Zlib-ng**

To compile using zlib-ng use the following cmake args:

```
-DZLIB_REPOSITORY=https://github.com/zlib-ng/zlib-ng -DZLIB_TAG=develop
```
**Compiling and Installing Zlib (Windows)**

To compile and install zlib to the Program Files directory with an Administrator command prompt:

```
cmake -DCMAKE_INSTALL_PREFIX="C:\Program Files (x86)\zlib" .
cmake --build . --config Release --target INSTALL
```
**Configure Existing Zlib Installation (Windows)**

To configure cmake with an existing zlib installation point cmake to your install directories:

```
cmake -DZLIB_LIBRARY:FILEPATH="C:\Program Files (x86)\zlib\lib\zlibstaticd.lib" .
cmake -DZLIB_INCLUDE_DIR:PATH="C:\Program Files (x86)\zlib\include" .
```

## Upgrading from 1.x

If you are using CMAKE it will automatically include all the files and define all the #defines
required based on your configuration and it will also build the project files necessary for your platform.

However, for some projects it may be necessary to drop in the new files into an existing project. In that
instance, some #defines will have to be set as they have changed.

|1.x|2.x|Description|
|-|-|:-|
||HAVE_ZLIB|Compile with ZLIB library. Older versions of Minizip required ZLIB. It is now possible to alternatively compile only using liblzma library.|
||HAVE_LZMA|Compile with LZMA support.|
|HAVE_BZIP2|HAVE_BZIP2|Compile with BZIP2 library support.|
|HAVE_APPLE_COMPRESSION|HAVE_LIBCOMP|Compile using Apple Compression library.|
|HAVE_AES|HAVE_WZAES|Compile using AES encryption support.|
||HAVE_PKCRYPT|Compile using PKWARE traditional encryption support. Previously this was automatically assumed.|
|NOUNCRYPT|Nearest to MZ_ZIP_NO_ENCRYPTION|Previously turn off all decryption support.|
|NOCRYPT|Nearest to MZ_ZIP_NO_ENCRYPTION|Previously turned off all encryption support.|
||MZ_ZIP_NO_ENCRYPTION|Turns off all encryption/decryption support.|
|NO_ADDFILEINEXISTINGZIP||Not currently supported.|
|IOWIN32_USING_WINRT_API|MZ_WINRT_API|Enable WinRT API support in Win32 file I/O stream.|
||MZ_ZIP_NO_COMPRESSION|Intended to reduce compilation size if not using zipping functionality.|
||MZ_ZIP_NO_COMPRESSION|Intended to reduce compilation size if not using zipping functionality.|

At a minimum HAVE_ZLIB and HAVE_PKCRYPT will be necessary to be defined for drop-in replacement. To determine which files to drop in, see the Contents section of the [README](https://github.com/zlib-ng/minizip-ng/blob/master/README.md).

## Security Considerations

### WinZip AES <!-- omit in toc -->

When compressing an archive with WinZIP AES enabled, by default it uses 256 bit encryption. During decompression whatever bit encryption was specified when the entry was added to the archive will be used.

WinZip AES encryption uses CTR on top of ECB which prevents identical ciphertext blocks that might occur when using ECB by itself. More details about the WinZIP AES format can be found in the [winzip documentation](zip/winzip_aes.md).

### How to Create a Secure Zip <!-- omit in toc -->

In order to create a secure zip file you must:

* Use WinZIP AES encryption
* Zip the central directory
* Sign the zip file using a certificate

The combination of using AES encryption and zipping the central directory prevents data leakage through filename exposure.
