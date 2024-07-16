# OS LIBRARY

set(OSBASE_SRC
    # cmake-format: sortable
    Application/CameraController.cpp
    Application/Fonts/FontSystem.cpp
    Application/Fonts/stbtt.cpp
    Application/InputSystem.cpp
    Application/Profiler/GpuProfiler.cpp
    Application/Profiler/ProfilerBase.cpp
    Application/RemoteControl.cpp
    Application/Screenshot.cpp
    Application/ThirdParty/OpenSource/imgui/imgui.cpp
    Application/ThirdParty/OpenSource/imgui/imgui_demo.cpp
    Application/ThirdParty/OpenSource/imgui/imgui_draw.cpp
    Application/ThirdParty/OpenSource/imgui/imgui_tables.cpp
    Application/ThirdParty/OpenSource/imgui/imgui_widgets.cpp
    Application/UI/UI.cpp
    Game/Scripting/LuaManager.cpp
    Game/Scripting/LuaManagerImpl.cpp
    Game/Scripting/LuaSystem.cpp
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lapi.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lauxlib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lbaselib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lbitlib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lcode.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lcorolib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lctype.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ldblib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ldebug.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ldo.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ldump.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lfunc.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lgc.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/linit.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/liolib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/llex.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lmathlib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lmem.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/loadlib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lobject.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lopcodes.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/loslib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lparser.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lstate.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lstring.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lstrlib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ltable.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ltablib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/ltm.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lundump.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lutf8lib.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lvm.c
    Game/ThirdParty/OpenSource/lua-5.3.5/src/lzio.c
    OS/CPUConfig.cpp
    OS/ThirdParty/OpenSource/cpu_features/src/impl_aarch64_linux_or_android.c
    OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_linux_or_android.c
    OS/WindowSystem/WindowSystem.cpp
    Resources/AnimationSystem/Animation/AnimatedObject.cpp
    Resources/AnimationSystem/Animation/Animation.cpp
    Resources/AnimationSystem/Animation/Clip.cpp
    Resources/AnimationSystem/Animation/ClipController.cpp
    Resources/AnimationSystem/Animation/ClipMask.cpp
    Resources/AnimationSystem/Animation/Rig.cpp
    Resources/AnimationSystem/Animation/SkeletonBatcher.cpp
    Tools/Network/Network.c
    Tools/ReloadServer/ReloadClient.cpp
    Utilities/FileSystem/FileSystem.c
    Utilities/FileSystem/SystemRun.c
    Utilities/Log/Log.c
    Utilities/Math/Algorithms.c
    Utilities/Math/StbDs.c
    Utilities/MemoryTracking/MemoryTracking.c
    Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c
    Utilities/ThirdParty/OpenSource/lz4/lz4.c
    Utilities/ThirdParty/OpenSource/zstd/common/debug.c
    Utilities/ThirdParty/OpenSource/zstd/common/entropy_common.c
    Utilities/ThirdParty/OpenSource/zstd/common/error_private.c
    Utilities/ThirdParty/OpenSource/zstd/common/fse_decompress.c
    Utilities/ThirdParty/OpenSource/zstd/common/pool.c
    Utilities/ThirdParty/OpenSource/zstd/common/threading.c
    Utilities/ThirdParty/OpenSource/zstd/common/xxhash.c
    Utilities/ThirdParty/OpenSource/zstd/common/zstd_common.c
    Utilities/ThirdParty/OpenSource/zstd/decompress/huf_decompress.c
    Utilities/ThirdParty/OpenSource/zstd/decompress/zstd_ddict.c
    Utilities/ThirdParty/OpenSource/zstd/decompress/zstd_decompress.c
    Utilities/ThirdParty/OpenSource/zstd/decompress/zstd_decompress_block.c
    Utilities/Threading/ThreadSystem.c
    Utilities/Timer.c
)

if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(XRANDR REQUIRED xrandr)
    find_package(X11 REQUIRED)

    list(
        APPEND
        OSBASE_SRC
        OS/Linux/LinuxBase.cpp
        OS/Linux/LinuxFileSystem.c
        OS/Linux/LinuxLog.c
        OS/Linux/LinuxThread.c
        OS/Linux/LinuxTime.c
        OS/Linux/LinuxWindow.cpp
        Utilities/FileSystem/UnixFileSystem.c
        Utilities/ThirdParty/OpenSource/zstd/decompress/huf_decompress_amd64.S
    )
endif()

if(WIN32)
    list(
        APPEND
        OSBASE_SRC
        OS/Windows/WindowsBase.cpp
        OS/Windows/WindowsFileSystem.cpp
        OS/Windows/WindowsLog.c
        OS/Windows/WindowsThread.c
        OS/Windows/WindowsTime.c
        OS/Windows/WindowsWindow.cpp
        OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_windows.c
    )
endif()

if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZ_CORE_FRAMEWORK QuartzCore REQUIRED)

    if(IOS)
        find_library(UIKIT_FRAMEWORK UIKit REQUIRED)

        list(
            APPEND
            OSBASE_SRC
            OS/Darwin/iOSBase.mm
            OS/Darwin/CocoaFileSystem.mm
            OS/Darwin/DarwinLog.c
            OS/Darwin/DarwinThread.c
            OS/Darwin/iOSWindow.mm
            Utilities/FileSystem/UnixFileSystem.c
            OS/ThirdParty/OpenSource/cpu_features/src/impl_aarch64_iOS.c
        )
    else()
        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)

        list(
            APPEND
            OSBASE_SRC
            OS/Darwin/macOSBase.mm
            OS/Darwin/CocoaFileSystem.mm
            OS/Darwin/DarwinLog.c
            OS/Darwin/DarwinThread.c
            OS/Darwin/macOSWindow.mm
            Utilities/FileSystem/UnixFileSystem.c
            OS/ThirdParty/OpenSource/cpu_features/src/impl_aarch64_iOS.c
        )
    endif()
    set(CXX_AS_OBJCXX_SRC
        # cmake-format: sortable
        Application/Fonts/FontSystem.cpp
        Application/InputSystem.cpp
        Application/Profiler/GpuProfiler.cpp
        Application/Profiler/ProfilerBase.cpp
        Application/RemoteControl.cpp
        Application/Screenshot.cpp
        Application/UI/UI.cpp
        Game/Scripting/LuaSystem.cpp
        OS/WindowSystem/WindowSystem.cpp
        Resources/AnimationSystem/Animation/SkeletonBatcher.cpp
    )
    set_source_files_properties(${CXX_AS_OBJCXX_SRC} PROPERTIES COMPILE_FLAGS "-x objective-c++")

    set(OBJCXX_ARC OS/Darwin/macOSWindow.mm)
    set_source_files_properties(${OBJCXX_ARC} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

if(ANDROID)
    find_library(LOG_LIBRARY log)
    find_library(
        VULKAN_LIBRARY
        NAMES vulkan
        PATHS ${ANDROID_NDK}/sources/third_party/vulkan/src/build-android/jniLibs/${ANDROID_ABI}
    )

    list(
        APPEND
        OSBASE_SRC
        OS/Android/AndroidBase.cpp
        OS/Android/AndroidFileSystem.cpp
        Utilities/FileSystem/UnixFileSystem.c
        OS/Android/AndroidLog.c
        OS/Android/AndroidThread.c
        OS/Android/AndroidTime.c
        OS/Android/AndroidWindow.cpp
        OS/ThirdParty/OpenSource/cpu_features/src/hwcaps.c
    )
endif()

add_library(OS STATIC ${OSBASE_SRC})
target_link_libraries(
    OS
    PUBLIC
        gainput
        ozz_base
        ozz_animation
        $<$<PLATFORM_ID:Linux>:${GTK3_LIBRARIES}>
        $<$<PLATFORM_ID:Linux>:${X11_LIBRARIES}>
        $<$<PLATFORM_ID:Linux>:${XRANDR_LIBRARIES}>
        $<$<PLATFORM_ID:Darwin>:${COREFOUNDATION_FRAMEWORK}>
        $<$<PLATFORM_ID:Darwin>:${APPKIT_FRAMEWORK}>
        $<$<PLATFORM_ID:Darwin>:${METAL_FRAMEWORK}>
        $<$<PLATFORM_ID:Darwin>:${QUARTZ_CORE_FRAMEWORK}>
        $<$<PLATFORM_ID:iOS>:${UIKIT_FRAMEWORK}>
        $<$<PLATFORM_ID:Android>:${LOG_LIBRARY}>
        $<$<PLATFORM_ID:Android>:${VULKAN_LIBRARY}>
        $<$<PLATFORM_ID:Android>:${CMAKE_SOURCE_DIR}/Common_3/OS/ThirdParty/OpenSource/agdk/libs/arm64-v8a_cpp_static_Release/libmemory_advice_static.a>
        $<$<PLATFORM_ID:Android>:${CMAKE_SOURCE_DIR}/Common_3/OS/ThirdParty/OpenSource/agdk/libs/arm64-v8a_cpp_static_Release/libswappy_static.a>
)
target_include_directories(
    OS PUBLIC $<$<PLATFORM_ID:Linux>:${GTK3_INCLUDE_DIRS}> Utilities/ThirdParty/OpenSource/TaskScheduler/Scheduler/Include/
              $<$<PLATFORM_ID:Android>:${CMAKE_SOURCE_DIR}/Common_3/OS/ThirdParty/OpenSource/agdk/include/>
)
if(WIN32)
    target_compile_definitions(OS PUBLIC UNICODE)
endif()

# RENDERER LIBRARY

include(TheForgeUtils)

set(RENDERER_SRC
    # cmake-format: sortable
    Game/ThirdParty/OpenSource/flecs/flecs.c
    Graphics/CommonShaderReflection.cpp
    Graphics/GraphicsConfig.cpp
    Graphics/PickRenderingAPI.cpp
    Renderer/ParticleSystem/ParticleSystem.cpp
    Renderer/VisibilityBuffer/VisibilityBuffer.cpp
    Resources/ResourceLoader/ResourceLoader.cpp
)

if(LINUX)
    list(APPEND RENDERER_SRC Graphics/Vulkan/Vulkan.cpp Graphics/Vulkan/VulkanRaytracing.cpp Graphics/Vulkan/VulkanShaderReflection.cpp)
endif()

if(WIN32)
    list(
        APPEND
        RENDERER_SRC
        Graphics/Vulkan/Vulkan.cpp
        Graphics/Vulkan/VulkanRaytracing.cpp
        Graphics/Vulkan/VulkanShaderReflection.cpp
        Graphics/Direct3D12/Direct3D12.cpp
        Graphics/Direct3D12/Direct3D12Raytracing.cpp
        Graphics/Direct3D12/Direct3D12ShaderReflection.cpp
        Graphics/Direct3D12/Direct3D12Hooks.cpp
        Graphics/Direct3D11/Direct3D11.cpp
        Graphics/Direct3D11/Direct3D11ShaderReflection.cpp
    )
endif()

if(APPLE)
    find_library(MPS_FRAMEWORK MetalPerformanceShaders)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)

    list(APPEND RENDERER_SRC Graphics/Metal/MetalRenderer.mm Graphics/Metal/MetalRaytracing.mm Graphics/Metal/MetalShaderReflection.mm)
    set_source_files_properties(${RENDERER_SRC} PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")

    set(C_AS_CXX_SRC # cmake-format: sortable
                     Game/ThirdParty/OpenSource/flecs/flecs.c
    )
    set_source_files_properties(${C_AS_CXX_SRC} PROPERTIES COMPILE_FLAGS "-x c")
endif()

if(ANDROID)
    find_library(EGL_LIBRARY EGL)
    find_library(GLESv2_LIBRARY GLESv2)

    list(
        APPEND
        RENDERER_SRC
        Graphics/Vulkan/Vulkan.cpp
        Graphics/Vulkan/VulkanRaytracing.cpp
        Graphics/Vulkan/VulkanShaderReflection.cpp
        Graphics/OpenGLES/GLES.cpp
        Graphics/OpenGLES/GLESShaderReflection.cpp
        Graphics/OpenGLES/EGLContextCreator.cpp
    )
endif()

add_library(Renderer STATIC ${RENDERER_SRC})
target_link_libraries(
    Renderer
    PUBLIC SpirVTools
           OS
           $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/Graphics/ThirdParty/OpenSource/ags/ags_lib/lib/amd_ags_x64.lib>
           $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/Graphics/ThirdParty/OpenSource/nvapi/amd64/nvapi64.lib>
           $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/lib/x64/dxcompiler.lib>
           $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/OS/ThirdParty/OpenSource/winpixeventruntime/bin/WinPixEventRuntime.lib>
           $<$<OR:$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:iOS>>:${MPS_FRAMEWORK}>
           $<$<PLATFORM_ID:iOS>:${COREGRAPHICS_FRAMEWORK}>
           $<$<PLATFORM_ID:iOS>:${METAL_FRAMEWORK}>
           $<$<PLATFORM_ID:iOS>:${QUARTZ_CORE_FRAMEWORK}>
           $<$<PLATFORM_ID:iOS>:${COREVIDEO_FRAMEWORK}>
           $<$<PLATFORM_ID:Android>:${EGL_LIBRARY}>
           $<$<PLATFORM_ID:Android>:${GLESv2_LIBRARY}>
)

if(WIN32)
    target_compile_definitions(Renderer PUBLIC D3D12_AGILITY_SDK_VERSION=611 UNICODE)
endif()

tf_add_shader(Renderer ${CMAKE_CURRENT_SOURCE_DIR}/Application/Fonts/Shaders/FSL/Fonts_ShaderList.fsl)
tf_add_shader(Renderer ${CMAKE_CURRENT_SOURCE_DIR}/Application/UI/Shaders/FSL/UI_ShaderList.fsl)
tf_add_shader(Renderer ${CMAKE_CURRENT_SOURCE_DIR}/Resources/AnimationSystem/Animation/Shaders/FSL/Animation_ShaderList.fsl)
