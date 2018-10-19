# Set compilers settings for all platforms/compilers.
#---------------------------------------------------

#-----------------
# Includes modules
include(CheckIncludeFiles)

#------------------------------
# Enables IDE folders y default
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#------------------------
# Available build options

#------------------------
# Lists all the cxx flags
set(cxx_all_flags
  CMAKE_CXX_FLAGS
  CMAKE_C_FLAGS
  CMAKE_CXX_FLAGS_DEBUG
  CMAKE_C_FLAGS_DEBUG
  CMAKE_CXX_FLAGS_MINSIZEREL
  CMAKE_C_FLAGS_MINSIZEREL
  CMAKE_CXX_FLAGS_RELWITHDEBINFO
  CMAKE_C_FLAGS_RELWITHDEBINFO
  CMAKE_CXX_FLAGS_RELEASE
  CMAKE_C_FLAGS_RELEASE)

#--------------------------------------
# Cross compiler compilation flags

if(ozz_build_cpp11)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Simd math force ref
if(ozz_build_simd_ref)
  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS OZZ_BUILD_SIMD_REF)
endif()

#--------------------------------------
# Modify default MSVC compilation flags
if(MSVC)

  #---------------------------
  # For the common build flags

  # Disables crt secure warnings
  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS)

  # Adds support for multiple processes builds
  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "/MP")

  # Set the warning level to W4
  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "/W4")

  # Set warning as error
  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "/WX")

  #---------------
  # Select whether to use the DLL version or the static library version of the Visual C++ runtime library.
  foreach(flag ${cxx_all_flags})
    if (ozz_build_msvc_rt_dll)
      string(REGEX REPLACE "/MT" "/MD" ${flag} "${${flag}}")
    else()
      string(REGEX REPLACE "/MD" "/MT" ${flag} "${${flag}}")
    endif()
  endforeach()

#--------------------------------------
# else consider the compiler as GCC compatible
# Modify default GCC compilation flags
else()

  # Set the warning level to Wall
  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "-Wall")

  # Enable extra level of warning
  #set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "-Wextra")

  # Template arguments cannot have the aligned attributes
  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "-Wno-ignored-attributes")

  # Null pointer dereferencing (required by fbx)
  if(ozz_build_fbx)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "-Wno-null-dereference")
  endif()

  # Set warning as error
  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "-Werror")

  #----------------------
  # Enables debug glibcxx if NDebug isn't defined, not supported by APPLE
  if(NOT APPLE)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_GLIBCXX_DEBUG>")
  endif()

  #----------------------
  # Sets emscripten output
  if(EMSCRIPTEN)
    SET(CMAKE_EXECUTABLE_SUFFIX ".html")

    #if(NOT ozz_build_simd_ref)
    #  set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "-msse2")
    #endif()
  endif()

endif()

#---------------------
# Prints all the flags
message(STATUS "---------------------------------------------------------")
message(STATUS "Default build type is: ${CMAKE_BUILD_TYPE}")
message(STATUS "The following compilation flags will be used:")
foreach(flag ${cxx_all_flags})
  message(${flag} " ${${flag}}")
endforeach()

message(STATUS "---------------------------------------------------------")

get_directory_property(DirectoryCompileOptions DIRECTORY ${PROJECT_SOURCE_DIR} COMPILE_OPTIONS)
message(STATUS "Directory Compile Options:")
foreach(opt ${DirectoryCompileOptions})
  message(STATUS ${opt})
endforeach()

message(STATUS "---------------------------------------------------------")

get_directory_property(DirectoryCompileDefinitions DIRECTORY ${PROJECT_SOURCE_DIR} COMPILE_DEFINITIONS)
message(STATUS "Directory Compile Definitions:")
foreach(def ${DirectoryCompileDefinitions})
  message(STATUS ${def})
endforeach()

message(STATUS "---------------------------------------------------------")

#----------------------------------------------
# Modifies output directory for all executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ".")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ".")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ".")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ".")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ".")

#-------------------------------
# Set a postfix for output files
set(CMAKE_DEBUG_POSTFIX "_d")
set(CMAKE_RELEASE_POSTFIX "_r")
set(CMAKE_MINSIZEREL_POSTFIX "_rs")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rd")
