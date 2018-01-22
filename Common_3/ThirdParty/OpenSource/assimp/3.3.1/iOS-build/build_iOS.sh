#/bin/bash
	#-DCMAKE_SYSTEM_NAME="Darwin" \
	#-DCMAKE_SYSTEM_VERSION="13" \
SDKROOT=$(xcrun -sdk iphoneos --show-sdk-path)
cmake \
	-DCMAKE_OSX_ARCHITECTURES="arm64" \
	-DCMAKE_CXX_COMPILER_WORKS="True" \
	-DCMAKE_C_COMPILER_WORKS="True" \
	-DASSIMP_BUILD_TESTS=0 \
	-DASSIMP_BUILD_ASSIMP_TOOLS=0 \
	-DBUILD_SHARED_LIBS=0 \
	-DCMAKE_OSX_SYSROOT=$SDKROOT \
	-DASSIMP_BUILD_ZLIB=1 \
	-DCMAKE_C_FLAGS="-isysroot $SDKROOT" \
	-DCMAKE_CXX_FLAGS="-isysroot $SDKROOT" \
	-DCMAKE_C_COMPILER=$(xcrun -sdk $SDKROOT -find clang) \
	-DCMAKE_CXX_COMPILER=$(xcrun -sdk $SDKROOT -find clang++) \
	CMakeLists.txt -G 'Xcode' ..
xcodebuild -config Release
