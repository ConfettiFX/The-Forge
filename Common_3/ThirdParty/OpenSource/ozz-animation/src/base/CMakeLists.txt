add_library(ozz_base STATIC
  ${PROJECT_SOURCE_DIR}/include/ozz/base/endianness.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/gtest_helper.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/memory/allocator.h
  memory/allocator.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/platform.h
  platform.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/log.h
  log.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/intrusive_list.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/deque.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/list.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/map.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/queue.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/set.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/stack.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/string.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/string_archive.h
  containers/string_archive.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/vector.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/vector_archive.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/containers/std_allocator.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/io/archive.h
  io/archive.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/io/archive_traits.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/io/stream.h
  io/stream.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/box.h
  maths/box.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/gtest_math_helper.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/internal/simd_math_config.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/internal/simd_math_ref-inl.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/internal/simd_math_sse-inl.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/math_ex.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/math_constant.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/quaternion.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/rect.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/simd_math.h
  maths/simd_math.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/soa_float.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/soa_quaternion.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/soa_transform.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/soa_float4x4.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/transform.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/vec_float.h
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/math_archive.h
  maths/math_archive.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/soa_math_archive.h
  maths/soa_math_archive.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/base/maths/simd_math_archive.h
  maths/simd_math_archive.cc)
target_include_directories(ozz_base PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
set_target_properties(ozz_base PROPERTIES FOLDER "ozz")

install(TARGETS ozz_base DESTINATION lib)

fuse_target("ozz_base")
