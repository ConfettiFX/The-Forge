cmake_minimum_required(VERSION 3.14)

project(The-Forge LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_UNIT_TESTS "Build The-Forge's unit tests" ON)
option(BUILD_ASSETPIPELINECMD "Build The-Forge's asset pipeline command" ON)

if(BUILD_UNIT_TESTS AND NOT EXISTS ${CMAKE_SOURCE_DIR}/Art)
    message(STATUS "Downloading art assets, please grab a cup of coffee and wait a few minutes. This will execute only once!")
    if(WIN32)
        execute_process(COMMAND ${CMAKE_SOURCE_DIR}/PRE_BUILD.bat)
    else()
        execute_process(COMMAND chmod +x ${CMAKE_SOURCE_DIR}/PRE_BUILD.command)
        execute_process(COMMAND ${CMAKE_SOURCE_DIR}/PRE_BUILD.command)
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

add_subdirectory(Common_3)
add_subdirectory(Common_3/Application/ThirdParty/OpenSource/gainput)
add_subdirectory(Common_3/Graphics/ThirdParty/OpenSource/SPIRV_Cross)
add_subdirectory(Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation)

if(BUILD_UNIT_TESTS)
    add_subdirectory(Examples_3/Unit_Tests/src)
endif()

if(BUILD_ASSETPIPELINECMD
   AND NOT ANDROID
   AND NOT IOS
)
    add_subdirectory(Common_3/Tools/BunyArchive/)
    add_subdirectory(Common_3/Tools/ThirdParty/OpenSource/ISPCTextureCompressor/)
    add_subdirectory(Common_3/Tools/AssetPipeline/)
endif()
