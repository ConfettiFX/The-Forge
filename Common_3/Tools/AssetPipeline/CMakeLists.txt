set(ASSETPIPELINE_CMD_SRC
    src/AssetPipeline_Textures.cpp
    src/AssetPipelineCmd.cpp
    src/AssetPipeline.cpp
    ../ThirdParty/OpenSource/meshoptimizer/src/vfetchoptimizer.cpp
    ../ThirdParty/OpenSource/meshoptimizer/src/vcacheoptimizer.cpp
    ../ThirdParty/OpenSource/meshoptimizer/src/overdrawoptimizer.cpp
    ../ThirdParty/OpenSource/meshoptimizer/src/indexgenerator.cpp
    ../ThirdParty/OpenSource/meshoptimizer/src/clusterizer.cpp
    ../ThirdParty/OpenSource/meshoptimizer/src/allocator.cpp
    ../../Resources/AnimationSystem/ThirdParty/OpenSource/TressFX/TressFXAsset.cpp
    ../../Utilities/FileSystem/ToolFileSystem.c
)

if(LINUX)
    list(APPEND ASSETPIPELINE_CMD_SRC ../../OS/Linux/LinuxToolsFileSystem.c)
endif()

if(WIN32)
    list(APPEND ASSETPIPELINE_CMD_SRC ../../OS/Windows/WindowsToolsFileSystem.cpp)
endif()

if(APPLE)
    list(APPEND ASSETPIPELINE_CMD_SRC ../../OS/Darwin/CocoaToolsFileSystem.mm)
    set(CXX_AS_OBJCXX_SRC # cmake-format: sortable
                          src/AssetPipeline.cpp src/AssetPipeline_Textures.cpp src/AssetPipelineCmd.cpp
    )
    set_source_files_properties(${CXX_AS_OBJCXX_SRC} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

add_executable(AssetPipelineCmd MACOSX_BUNDLE ${ASSETPIPELINE_CMD_SRC})

# Hack to fix wrong include directory for ILog.h at Common_3/Tools/ThirdParty/OpenSource/meshoptimizer/src/meshoptimizer.h:11
target_include_directories(
    AssetPipelineCmd PRIVATE ${CMAKE_SOURCE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/TaskScheduler/Scheduler/Include/
)

add_dependencies(AssetPipelineCmd ispc_texcomp)

target_link_libraries(AssetPipelineCmd PRIVATE OS Renderer ozz_animation ozz_animation_offline Buny ispc_texcomp)

# Define the path to the empty header file
set(EMPTY_CONFIG_HEADER "${CMAKE_BINARY_DIR}/empty_config.h")

# Create an empty file if it doesn't exist
if(NOT EXISTS ${EMPTY_CONFIG_HEADER})
    file(WRITE ${EMPTY_CONFIG_HEADER} "#define ENABLE_ASSET_PIPELINE_CGLTF_WRITE_IMPLEMENTATION\n")
endif()

# Set the EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH to the path of the empty file
target_compile_definitions(AssetPipelineCmd PRIVATE EXTERNAL_ASSET_PIPELINE_CONFIG_FILEPATH="${EMPTY_CONFIG_HEADER}")

if(WIN32)
    set_target_properties(AssetPipelineCmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
    add_custom_command(
        TARGET AssetPipelineCmd
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/ags/ags_lib/lib/amd_ags_x64.dll
            ${CMAKE_SOURCE_DIR}/Common_3/OS/ThirdParty/OpenSource/winpixeventruntime/bin/WinPixEventRuntime.dll
            ${CMAKE_SOURCE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/lib/x64/dxcompiler.lib
            ${CMAKE_SOURCE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64/dxcompiler.dll
            ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
        COMMENT "Copying AssetPipelineCmd dependencies"
    )
endif()
