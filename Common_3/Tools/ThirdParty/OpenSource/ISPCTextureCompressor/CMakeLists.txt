# Build ISPC files
if(LINUX)
    set(ISPC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/ISPC/linux/ispc)
endif()
if(WIN32)
    set(ISPC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/ISPC/win/ispc.exe)
endif()
if(APPLE)
    set(ISPC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/ISPC/osx/ispc)
endif()

set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/ispc_texcomp)
set(OBJROOT ${CMAKE_BINARY_DIR})

# Make sure the ISPC executable has the correct permissions
execute_process(COMMAND chmod 755 ${ISPC_EXECUTABLE})

# Set up file paths
set(KERNEL_ISPC "${SRCROOT}/kernel.ispc")
set(KERNEL_ASTC_ISPC "${SRCROOT}/kernel_astc.ispc")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    add_custom_command(
        OUTPUT ${OBJROOT}/kernel_ispc.o
        COMMAND ${ISPC_EXECUTABLE} ARGS -O2 ${KERNEL_ISPC} -o "${OBJROOT}/kernel_ispc.o" -h "${SRCROOT}/kernel_ispc.h" --arch=aarch64
                --target=neon-i32x4 --opt=fast-math
        COMMENT "Compiling ISPC kernel for ARM"
    )
    add_custom_command(
        OUTPUT ${OBJROOT}/kernel_astc_ispc.o
        COMMAND ${ISPC_EXECUTABLE} ARGS -O2 ${KERNEL_ASTC_ISPC} -o ${OBJROOT}/kernel_astc_ispc.o -h ${SRCROOT}/kernel_astc_ispc.h
                --arch=aarch64 --target=neon-i32x4 --opt=fast-math
        COMMENT "Compiling ASTC ISPC kernel for ARM"
    )
else()
    add_custom_command(
        OUTPUT ${OBJROOT}/kernel_ispc.o
        COMMAND ${ISPC_EXECUTABLE} ARGS -O2 ${KERNEL_ISPC} -o "${OBJROOT}/kernel_ispc.o" -h "${SRCROOT}/kernel_ispc.h" --arch=x86-64
                --target=sse2-i32x4 --opt=fast-math --pic
        COMMENT "Compiling ISPC kernel for x86_64"
    )
    add_custom_command(
        OUTPUT ${OBJROOT}/kernel_astc_ispc.o
        COMMAND ${ISPC_EXECUTABLE} ARGS -O2 ${KERNEL_ASTC_ISPC} -o ${OBJROOT}/kernel_astc_ispc.o -h ${SRCROOT}/kernel_astc_ispc.h
                --arch=x86-64 --target=sse2-i32x4 --opt=fast-math --pic
        COMMENT "Compiling ASTC ISPC kernel for x86_64"
    )
endif()

set(ISPC_TEXCOMP_SRC ispc_texcomp/ispc_texcomp.cpp ispc_texcomp/ispc_texcomp_astc.cpp ${OBJROOT}/kernel_ispc.o
                     ${OBJROOT}/kernel_astc_ispc.o
)
add_library(ispc_texcomp STATIC ${ISPC_TEXCOMP_SRC})
