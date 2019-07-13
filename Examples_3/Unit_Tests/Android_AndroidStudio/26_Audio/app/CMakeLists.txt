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
include_directories(${ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/sound)
add_library( native_app_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

set(COMMON_DIR ../../../../../Common_3)
set(MIDDLEWARE_DIR ../../../../../Middleware_3)
add_library(os STATIC
        # core common
        ${COMMON_DIR}/OS/Android/AndroidFileSystem.cpp
        ${COMMON_DIR}/OS/Android/AndroidBase.cpp
        ${COMMON_DIR}/OS/Android/AndroidLog.cpp
        ${COMMON_DIR}/OS/Android/AndroidThread.cpp
        ${COMMON_DIR}/OS/Android/AndroidTime.cpp
        ${COMMON_DIR}/OS/Camera/FpsCameraController.cpp
        ${COMMON_DIR}/OS/Camera/GuiCameraController.cpp
        ${COMMON_DIR}/OS/Core/FileSystem.cpp
        ${COMMON_DIR}/OS/Core/PlatformEvents.cpp
        ${COMMON_DIR}/OS/Core/ThreadSystem.cpp
        ${COMMON_DIR}/OS/Core/Timer.cpp
        ${COMMON_DIR}/OS/Image/Image.cpp
        ${COMMON_DIR}/OS/Logging/Log.cpp
        ${COMMON_DIR}/OS/Image/Image.cpp
        ${COMMON_DIR}/OS/Input/InputSystem.cpp
        ${COMMON_DIR}/OS/MemoryTracking/MemoryTracking.cpp
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
        ${MIDDLEWARE_DIR}/Animation/AnimatedObject.cpp
        ${MIDDLEWARE_DIR}/Animation/Animation.cpp
        ${MIDDLEWARE_DIR}/Animation/Clip.cpp
        ${MIDDLEWARE_DIR}/Animation/ClipController.cpp
        ${MIDDLEWARE_DIR}/Animation/ClipMask.cpp
        ${MIDDLEWARE_DIR}/Animation/Rig.cpp
        ${MIDDLEWARE_DIR}/Animation/SkeletonBatcher.cpp

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
target_include_directories(os PUBLIC ${MIDDLEWARE_DIR}/ECS)
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
        ${COMMON_DIR}/Tools/SpirvTools/SpirvTools.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp
        ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp
        )
target_compile_definitions(renderer PUBLIC VULKAN)
target_include_directories(renderer PUBLIC ${SHADERC_DIR}/include)
target_link_libraries(renderer os shaderc)

# build ga input
SET(GAINPUT_BUILD_SHARED OFF)
SET(GAINPUT_BUILD_STATIC ON)
add_subdirectory(../../../../../Common_3/ThirdParty/OpenSource/gainput/lib/ build)

SET(SoLoud_SRC ${COMMON_DIR}/ThirdParty/OpenSource/soloud20181119/src)
# build vulkan app
set(SRC_DIR ../../../src)
set(APP_NAME 26_Audio)
add_library(audio SHARED
        ${SRC_DIR}/${APP_NAME}/26_Audio.cpp

        # SoLoud
        ${SoLoud_SRC}/audiosource/monotone/soloud_monotone.cpp
        ${SoLoud_SRC}/audiosource/openmpt/soloud_openmpt.cpp
        ${SoLoud_SRC}/audiosource/openmpt/soloud_openmpt_dll.c
        ${SoLoud_SRC}/audiosource/sfxr/soloud_sfxr.cpp
        ${SoLoud_SRC}/audiosource/speech/darray.cpp
        ${SoLoud_SRC}/audiosource/speech/klatt.cpp
        ${SoLoud_SRC}/audiosource/speech/resonator.cpp
        ${SoLoud_SRC}/audiosource/speech/soloud_speech.cpp
        ${SoLoud_SRC}/audiosource/speech/tts.cpp
        ${SoLoud_SRC}/audiosource/tedsid/sid.cpp
        ${SoLoud_SRC}/audiosource/tedsid/soloud_tedsid.cpp
        ${SoLoud_SRC}/audiosource/tedsid/ted.cpp
        ${SoLoud_SRC}/audiosource/vic/soloud_vic.cpp
        ${SoLoud_SRC}/audiosource/vizsn/soloud_vizsn.cpp
        ${SoLoud_SRC}/audiosource/wav/dr_impl.cpp
        ${SoLoud_SRC}/audiosource/wav/soloud_wav.cpp
        ${SoLoud_SRC}/audiosource/wav/soloud_wavstream.cpp
        ${SoLoud_SRC}/audiosource/wav/stb_vorbis.cpp
        ${SoLoud_SRC}/backend/opensles/soloud_opensles.cpp

        ${SoLoud_SRC}/core/soloud.cpp
        ${SoLoud_SRC}/core/soloud_audiosource.cpp
        ${SoLoud_SRC}/core/soloud_bus.cpp
        ${SoLoud_SRC}/core/soloud_core_3d.cpp
        ${SoLoud_SRC}/core/soloud_core_basicops.cpp
        ${SoLoud_SRC}/core/soloud_core_faderops.cpp
        ${SoLoud_SRC}/core/soloud_core_filterops.cpp
        ${SoLoud_SRC}/core/soloud_core_getters.cpp
        ${SoLoud_SRC}/core/soloud_core_setters.cpp
        ${SoLoud_SRC}/core/soloud_core_voicegroup.cpp
        ${SoLoud_SRC}/core/soloud_core_voiceops.cpp
        ${SoLoud_SRC}/core/soloud_fader.cpp
        ${SoLoud_SRC}/core/soloud_fft.cpp
        ${SoLoud_SRC}/core/soloud_fft_lut.cpp
        ${SoLoud_SRC}/core/soloud_file.cpp
        ${SoLoud_SRC}/core/soloud_filter.cpp
        ${SoLoud_SRC}/core/soloud_queue.cpp
        ${SoLoud_SRC}/core/soloud_thread.cpp
        ${SoLoud_SRC}/filter/soloud_bassboostfilter.cpp
        ${SoLoud_SRC}/filter/soloud_biquadresonantfilter.cpp
        ${SoLoud_SRC}/filter/soloud_dcremovalfilter.cpp
        ${SoLoud_SRC}/filter/soloud_echofilter.cpp
        ${SoLoud_SRC}/filter/soloud_fftfilter.cpp
        ${SoLoud_SRC}/filter/soloud_flangerfilter.cpp
        ${SoLoud_SRC}/filter/soloud_lofifilter.cpp
        ${SoLoud_SRC}/filter/soloud_robotizefilter.cpp
        ${SoLoud_SRC}/filter/soloud_waveshaperfilter.cpp
        )

target_compile_definitions(audio PUBLIC VULKAN)
# ignore these warnings
# unused private field
# unused variable
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wno-unused-private-field -Wno-unused-variable -Wno-unused-function\
                     -DVK_USE_PLATFORM_ANDROID_KHR")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")


target_include_directories(audio PUBLIC ${COMMON_DIR}/OS/Interfaces
        ${COMMON_DIR}/ThirdParty/OpenSource/soloud20181119/include
        )
target_link_libraries(audio native_app_glue shaderc os renderer log vulkan android gainputstatic OpenSLES)



