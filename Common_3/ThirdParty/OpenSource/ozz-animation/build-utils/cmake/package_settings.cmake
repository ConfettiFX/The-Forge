# Setup packages informations.
#-----------------------------

# Prepares installation
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include" DESTINATION ".")

# Prepares packing
set(CPACK_PACKAGE_VERSION_MAJOR ${OZZ_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${OZZ_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${OZZ_VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR "Guillaume Blanc")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ozz run-time animation library and tools. http://github.com/guillaumeblanc/ozz-animation")
#set(CPACK_PACKAGE_ICON )
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.md")
set(CPACK_SOURCE_IGNORE_FILES
  "/build/"  # Out-of-source build directory.
  "/Testing/"  # CDash generated files.
  "/\\\\.git/" 
  ".*\\\\.kdev4"
  ".*~")
set(CPACK_NSIS_MENU_LINKS
  "http://github.com/guillaumeblanc/ozz-animation;Ozz home"
  "bin/samples;Samples"
  "bin/tools;Tools")

# Defines local variables used for packaging
STRING(SUBSTRING ${CMAKE_SYSTEM_NAME} 0 3 _PACKAGE_OS)
if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
  set(_PACKAGE_BITS "32")
else()
  set(_PACKAGE_BITS "64")
endif()
set(_PACKAGE_COMILER ${CMAKE_CXX_COMPILER_ID})
set(_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${_PACKAGE_VERSION}-${_PACKAGE_OS}${_PACKAGE_BITS}-${_PACKAGE_COMILER}")
include(CPack)
