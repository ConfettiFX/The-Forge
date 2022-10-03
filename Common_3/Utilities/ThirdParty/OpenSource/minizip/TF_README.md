This library is a composite of multiple versions of [minizip-ng](https://github.com/zlib-ng/minizip-ng). 
Specifically following versions were used:
+ [3.0.2](https://github.com/zlib-ng/minizip-ng/releases/tag/3.0.2) - as a main source of files
+ [2.10.1](https://github.com/zlib-ng/minizip-ng/releases/tag/2.10.1) - as a source for encryption implementation.
These versions were chosen because:
1. **3.0.2** has some bug fixes.
2. **2.10.1** has option to use small cryptography library(brg). Later versions of minizip require OpenSSL/Windows/Apple cryptography libraries. And maintenance of OpenSSL for all consoles is not a reasonable task.

Additional changes applied to the library:
+ All unused files were moved into `unused` directory.
+ `MZ_ALLOC`/`TF_FREE` were made to use `tf_alloc`/`tf_free`
+ All stream interfaces were refactored to follow our streams API defined in `OS/Interface/IFileSystem.h`

Integration instructions for new projects:
+ Add all source files from minizip and lib/brg
+ (optional) Add `miniz.h` header from zip library
+ Might require _CRT_SECURE_NO_WARNINGS preprocessor definition on Microsoft platforms
