cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_DEBUG_POSTFIX "")

# build native_app_glue as a static lib
set(SHADERC_DIR ${ANDROID_NDK}/sources/third_party/shaderc)
add_library (shaderc STATIC IMPORTED)
set_target_properties(shaderc PROPERTIES
        IMPORTED_LOCATION ${SHADERC_DIR}/libs/c++_static/${ANDROID_ABI}/libshaderc.a)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library( native_app_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

set(COMMON_DIR ../../../../../Common_3)
set(MIDDLEWARE_DIR ../../../../../Middleware_3)
add_library(os STATIC
        # core common
        ${COMMON_DIR}/OS/Android/AndroidFileSystem.cpp
        ${COMMON_DIR}/OS/Android/AndroidBase.cpp
        ${COMMON_DIR}/OS/Android/AndroidLogManager.cpp
        ${COMMON_DIR}/OS/Android/AndroidThreadManager.cpp
        ${COMMON_DIR}/OS/Camera/FpsCameraController.cpp
        ${COMMON_DIR}/OS/Camera/GuiCameraController.cpp
        ${COMMON_DIR}/OS/Core/FileSystem.cpp
        ${COMMON_DIR}/OS/Core/PlatformEvents.cpp
        ${COMMON_DIR}/OS/Core/ThreadSystem.cpp
        ${COMMON_DIR}/OS/Core/Timer.cpp
        ${COMMON_DIR}/OS/Core/Timer.cpp
        ${COMMON_DIR}/OS/Image/Image.cpp
        ${COMMON_DIR}/OS/Logging/LogManager.cpp
        ${COMMON_DIR}/OS/Image/Image.cpp
        ${COMMON_DIR}/OS/Input/InputSystem.cpp
        ${COMMON_DIR}/OS/MemoryTracking/MemoryTrackingManager.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/MicroProfile/Profiler.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/MicroProfile/ProfilerUI.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/MicroProfile/ProfilerInput.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/MicroProfile/ProfilerDraw.cpp

        # middleware
        ${MIDDLEWARE_DIR}/Text/Fontstash.cpp
        ${MIDDLEWARE_DIR}/UI/AppUI.cpp
        ${MIDDLEWARE_DIR}/UI/ImguiGUIDriver.cpp

        # imgui
        ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui_demo.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui_draw.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui_widgets.cpp

        # eastl
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/EAStdC/EAMemory.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/EAStdC/EASprintf.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/allocator_forge.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/assert.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/allocator_forge.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/fixed_pool.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/hashtable.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/intrusive_list.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/numeric_limits.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/red_black_tree.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/string.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/EASTL/thread_support.cpp
        )
target_compile_definitions(os PUBLIC VULKAN)
target_link_libraries(os shaderc)

add_library(renderer STATIC
        # vulkan renderer
        ${COMMON_DIR}/Renderer/Vulkan/Vulkan.cpp
        ${COMMON_DIR}/Renderer/Vulkan/VulkanRaytracing.cpp
        ${COMMON_DIR}/Renderer/Vulkan/VulkanShaderReflection.cpp
        ${COMMON_DIR}/Renderer/CommonShaderReflection.cpp
        ${COMMON_DIR}/Renderer/GpuProfiler.cpp
        ${COMMON_DIR}/Renderer/ResourceLoader.cpp

        # spirv tools
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp
        ${COMMON_DIR}/Tools/SpirvTools/SpirvTools.cpp
        )
target_compile_definitions(renderer PUBLIC VULKAN)
target_include_directories(renderer PUBLIC ${SHADERC_DIR}/include)
target_link_libraries(renderer os shaderc)

# build ga input
SET(GAINPUT_BUILD_SHARED OFF)
SET(GAINPUT_BUILD_STATIC ON)
add_subdirectory(../../../../../Common_3/ThirdParty/OpenSource/gainput/lib/ build)

# build vulkan app
set(SRC_DIR ../../../src)
set(APP_NAME 05_FontRendering)
add_library(fontrendering SHARED
        ${SRC_DIR}/${APP_NAME}/05_FontRendering.cpp
        )

target_compile_definitions(fontrendering PUBLIC VULKAN)
# ignore these warnings
# unused private field
# unused variable
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Werror -Wno-unused-private-field -Wno-unused-variable -Wno-unused-function\
                     -DVK_USE_PLATFORM_ANDROID_KHR")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")


target_include_directories(fontrendering PUBLIC ${COMMON_DIR}/OS/Interfaces)
target_link_libraries(fontrendering native_app_glue shaderc os renderer log vulkan android gainputstatic)



