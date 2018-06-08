INCLUDE(CMakeForceCompiler)

SET (CMAKE_CROSSCOMPILING   TRUE)
SET (CMAKE_SYSTEM_NAME      "Darwin")
SET (CMAKE_SYSTEM_PROCESSOR "arm64")
SET (CMAKE_C_COMPILER)
SET (CMAKE_CXX_COMPILER)

SET (SDKVER     "7.1")
SET (DEVROOT    "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain")
SET (SDKROOT    "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS${SDKVER}.sdk")
SET (CC "${DEVROOT}/usr/bin/clang")
SET (CXX "${DEVROOT}/usr/bin/clang++")

SET (CMAKE_FIND_ROOT_PATH               "${SDKROOT}" "${DEVROOT}")
SET (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM  NEVER)
SET (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
SET (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)