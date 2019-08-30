# mcpp

This is a fork of [mcpp](http://mcpp.sourceforge.net/). mcpp is a C/C++ preprocessor used by the [Ice](https://github.com/zeroc-ice/ice) Slice compilers.

This fork consists of a simplified version of mcpp 2.7.2 plus various fixes, tailored to the Slice compilers requirements. Its build system creates only a static library and no command-line preprocessor. The configure script was replaced by a custom `config.h` suitable for Windows, macOS and Linux builds. Tests and other optional files were removed.

The resulting static library is later used when creating the [Ice](https://github.com/zeroc-ice/ice) Slice shared library or statically-linked Slice compilers such as `slice2js`.

## Build Instructions

### Linux and macOS
Simply type:
```
  make
```  
This creates a static library `libmcpp.a`. Depending on your platform, the library is created in the following directory:

* `lib64` - Linux system with a `/usr/lib64` directory
* `lib` - Linux system without a `/usr/lib64` directory, macOS

### Windows with Visual Studio

Open a Visual Studio Command Prompt, and type:
```
  MSbuild msbuild\mcpp.vcxproj
```
This creates a static library `mcpp.lib` for Release configurations and `mcppd.lib` for Debug configurations.
You can select a specific Configuration (`Debug` or `Release`) and Platform (`Win32` or `x64`) as shown below:

```
  MSbuild msbuild\mcpp.vcxproj /p:Configuration=Release /p:Platform=x64
```
