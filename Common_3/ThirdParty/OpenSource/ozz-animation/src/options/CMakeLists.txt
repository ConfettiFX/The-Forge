add_library(ozz_options STATIC
  ../../include/ozz/options/options.h
  options.cc)
target_include_directories(ozz_options PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
set_target_properties(ozz_options PROPERTIES FOLDER "ozz")

install(TARGETS ozz_options DESTINATION lib)

fuse_target("ozz_options")
