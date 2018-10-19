add_library(ozz_geometry STATIC
  ${PROJECT_SOURCE_DIR}/include/ozz/geometry/runtime/skinning_job.h
  skinning_job.cc)
target_link_libraries(ozz_geometry
  ozz_base)
set_target_properties(ozz_geometry
  PROPERTIES FOLDER "ozz")

install(TARGETS ozz_geometry DESTINATION lib)

fuse_target("ozz_geometry")
