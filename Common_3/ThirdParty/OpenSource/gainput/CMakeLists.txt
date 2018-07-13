cmake_minimum_required(VERSION 2.8)
set(GAINPUT_MAJOR_VERSION 1)
set(GAINPUT_MINOR_VERSION 0)
set(GAINPUT_PATCH_VERSION 0)
set(GAINPUT_VERSION ${GAINPUT_MAJOR_VERSION}.${GAINPUT_MINOR_VERSION}.${GAINPUT_PATCH_VERSION})

option(GAINPUT_SAMPLES "Build Samples for Gainput" OFF)
option(GAINPUT_TESTS "Build Tests for Gainput" OFF)
option(GAINPUT_BUILD_SHARED "BUILD_SHARED" ON)
option(GAINPUT_BUILD_STATIC "BUILD_STATIC" ON)

if(!WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")
else()
	set(XINPUT "Xinput9_1_0")
	if ( ${CMAKE_SYSTEM_VERSION} LESS 6.1 )
		set(XINPUT, "xinput")
	endif()
endif()

if(ANDROID)
	include(extern/cmake/AndroidNdkModules.cmake)
	android_ndk_import_module_native_app_glue()
endif()

add_subdirectory(lib)

if(GAINPUT_SAMPLES)
	add_subdirectory(samples)
endif()

if(GAINPUT_TESTS)
	add_subdirectory(test)
endif()

