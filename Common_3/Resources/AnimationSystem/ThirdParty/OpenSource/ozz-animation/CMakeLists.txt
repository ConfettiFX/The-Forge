set(OZZ_BASE_SRC
    # cmake-format: sortable
    src/base/containers/string_archive.cc
    src/base/io/archive.cc
    src/base/maths/math_archive.cc
    src/base/maths/simd_math_archive.cc
    src/base/maths/soa_math_archive.cc
    src/base/memory/allocator.cc
    src/base/platform.cc
)

add_library(ozz_base STATIC ${OZZ_BASE_SRC})
target_include_directories(ozz_base PUBLIC $<$<PLATFORM_ID:Android>:${ANDROID_NDK}/sources/android/native_app_glue/>)

set(OZZ_ANIMATION_SRC
    # cmake-format: sortable
    src/animation/runtime/animation.cc
    src/animation/runtime/blending_job.cc
    src/animation/runtime/ik_aim_job.cc
    src/animation/runtime/ik_two_bone_job.cc
    src/animation/runtime/local_to_model_job.cc
    src/animation/runtime/sampling_job.cc
    src/animation/runtime/skeleton.cc
    src/animation/runtime/skeleton_utils.cc
    src/animation/runtime/track.cc
    src/animation/runtime/track_sampling_job.cc
    src/animation/runtime/track_triggering_job.cc
)

add_library(ozz_animation STATIC ${OZZ_ANIMATION_SRC})
target_link_libraries(ozz_animation ozz_base)

set(OZZ_ANIMATION_OFFLINE_SRC
    # cmake-format: sortable
    src/animation/offline/additive_animation_builder.cc
    src/animation/offline/animation_builder.cc
    src/animation/offline/animation_optimizer.cc
    src/animation/offline/raw_animation.cc
    src/animation/offline/raw_animation_archive.cc
    src/animation/offline/raw_animation_utils.cc
    src/animation/offline/raw_skeleton.cc
    src/animation/offline/raw_skeleton_archive.cc
    src/animation/offline/raw_track.cc
    src/animation/offline/skeleton_builder.cc
    src/animation/offline/track_builder.cc
    src/animation/offline/track_optimizer.cc
)

add_library(ozz_animation_offline STATIC ${OZZ_ANIMATION_OFFLINE_SRC})
target_include_directories(
    ozz_animation_offline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src $<$<PLATFORM_ID:Android>:${ANDROID_NDK}/sources/android/native_app_glue/>
)
