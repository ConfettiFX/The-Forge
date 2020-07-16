cmake_minimum_required(VERSION 3.15)
set(CMAKE_GENERATOR_PLATFORM x64)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

if(AGS_INTERNAL_DEBUG)
project(ags_sample_internal VERSION 5.4.1)
else()
project(ags_sample VERSION 5.4.1)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ags_lib/inc)

set(AGS_SOURCES src/AGSSample.cpp)
set(AGS_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/../ags_lib/inc/amd_ags.h)

if(AGS_INTERNAL_DEBUG)
add_subdirectory(../ags_lib ags_internal)
add_executable(${PROJECT_NAME} ${AGS_SOURCES} ${AGS_PUBLIC_HEADER})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../ags_lib/lib/amd_ags_x64_2019_MDd.lib)

else()
add_library(amd_ags SHARED IMPORTED GLOBAL)
set_property(TARGET amd_ags PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/../ags_lib/lib/amd_ags_x64.lib)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../ags_lib/lib/amd_ags_x64.dll DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(${PROJECT_NAME} ${AGS_SOURCES} ${AGS_PUBLIC_HEADER})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC amd_ags)
endif()



set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX d)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

source_group("Source"                           FILES ${AGS_SOURCES}) 
source_group("Inc"              				FILES ${AGS_PUBLIC_HEADER})
