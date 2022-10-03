# Library usage example

This is a minimal example of using the astcenc codec as a library in another
project. This sample shows:

  * How to include astcenc as an external project CMake dependency.
  * How to use the API to compress and decompress an image.

For sake of simplicity the example application uses fixed compression settings,
reading an uncompressed LDR image, compressing using 6x6 blocks at medium
quality, and then decompressing and writing the decompressed image back to disk
as a PNG file.

## Building

:warning: For sake of simplicity the example CMake project uses the CMake
`ExternalProject` mechanism to import the astcenc project from GitHub. This is
trivial to integrate, but requires network access during the build to pull the
astcenc project.

Most users will want to store a copy of astcenc in a project sub-directory,
e.g. by using git submodules, and then use `add_subdirectory()` to include the
project in their build. This allows the user to directly use the astcenc core
library as a link requirement via `target_link_libraries()`, without the
additional plumbing that `ExternalProject` requires.

### Linux and macOS

From the `./Utils/Example` directory.

```
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
make -j8
```

### Windows

From the `./Utils/Example` directory, in a Visual Studio command prompt.

```
mkdir build
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake
```

## Running

From the build directory above.

```
astcenc_example <input.png> <output.png>
```
