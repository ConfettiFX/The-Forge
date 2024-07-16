# ios.toolchain.cmake
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_SYSROOT iphoneos)

# Specify the minimum iOS version
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.0")

# Set the architecture(s) to build for
set(CMAKE_OSX_ARCHITECTURES "arm64")

# Disable codesign for the build (optional)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
