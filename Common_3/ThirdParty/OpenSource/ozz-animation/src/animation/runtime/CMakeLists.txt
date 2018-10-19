add_library(ozz_animation STATIC
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/animation.h
  animation.cc
  animation_keyframe.h
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/blending_job.h
  blending_job.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/local_to_model_job.h
  local_to_model_job.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/sampling_job.h
  sampling_job.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/skeleton.h
  skeleton.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/skeleton_utils.h
  skeleton_utils.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/track.h
  track.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/track_sampling_job.h
  track_sampling_job.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/track_triggering_job.h
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/runtime/track_triggering_job_stl.h
  track_triggering_job.cc)
target_link_libraries(ozz_animation
  ozz_base)

set_target_properties(ozz_animation
  PROPERTIES FOLDER "ozz")

install(TARGETS ozz_animation DESTINATION lib)

fuse_target("ozz_animation")
