set(SPIRVTOOLS_SRC
    spirv_cfg.cpp
    spirv_cpp.cpp
    spirv_cross.cpp
    spirv_cross_parsed_ir.cpp
    spirv_cross_util.cpp
    spirv_glsl.cpp
    spirv_hlsl.cpp
    spirv_msl.cpp
    spirv_parser.cpp
    spirv_reflect.cpp
    SpirvTools.cpp
)

add_library(SpirVTools STATIC ${SPIRVTOOLS_SRC})

target_include_directories(SpirVTools PUBLIC $<$<PLATFORM_ID:Android>:${ANDROID_NDK}/sources/android/native_app_glue/>)
