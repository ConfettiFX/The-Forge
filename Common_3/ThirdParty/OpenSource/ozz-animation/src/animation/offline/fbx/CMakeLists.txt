if(NOT ozz_build_fbx)
  return()
endif()

add_library(ozz_animation_fbx STATIC
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/offline/fbx/fbx.h
  fbx.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/offline/fbx/fbx_animation.h
  fbx_animation.cc
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/offline/fbx/fbx_skeleton.h
  fbx_skeleton.cc)
target_include_directories(ozz_animation_fbx PUBLIC
  $<BUILD_INTERFACE:${FBX_INCLUDE_DIRS}>)
target_link_libraries(ozz_animation_fbx
  ozz_animation_offline
  debug ${FBX_LIBRARIES_DEBUG}
  optimized ${FBX_LIBRARIES})
set_target_properties(ozz_animation_fbx
  PROPERTIES FOLDER "ozz/tools")

install(TARGETS ozz_animation_fbx DESTINATION lib)

fuse_target("ozz_animation_fbx")

add_executable(fbx2ozz
  fbx2ozz.h
  fbx2ozz.cc
  fbx2ozz_anim.cc
  fbx2ozz_skel.cc)
target_link_libraries(fbx2ozz
  ozz_animation_tools
  ozz_animation_fbx)
set_target_properties(fbx2ozz
  PROPERTIES FOLDER "ozz/tools")

install(TARGETS fbx2ozz DESTINATION bin/tools)

# Gerenate binary data to a specific folder (${ozz_media_directory}/bin)
set(build_configurations
  # Library data
  "/fbx/alain/skeleton.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":true}},\"animations\":[]}\;output:alain_skeleton.ozz"
  "/fbx/alain/atlas.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"alain_atlas_raw.ozz\",\"raw\":true,\"optimize\":false}]}\;output:alain_atlas_raw.ozz\;depend:alain_skeleton.ozz"
  "/fbx/alain/locomotions.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"alain_*.ozz\"}]}\;output:alain_walk.ozz\;output:alain_jog.ozz\;output:alain_run.ozz\;depend:alain_skeleton.ozz"
  "/fbx/alain/crossarms.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"alain_crossarms.ozz\"}]}\;output:alain_crossarms.ozz\;depend:alain_skeleton.ozz"
  "/fbx/alain/crackhead.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"alain_crackhead.ozz\"},{\"filename\":\"alain_crackhead_additive.ozz\",\"additive\":true}]}\;output:alain_crackhead_additive.ozz\;output:alain_crackhead.ozz\;depend:alain_skeleton.ozz"
  
  # Robot user channels
  "/fbx/robot.fbx\;\;config_file:${PROJECT_SOURCE_DIR}/samples/user_channel/config.json\;output:robot_skeleton.ozz\;output:robot_animation.ozz\;output:robot_track_grasp.ozz"

  # Baked animation
  "/fbx/baked.fbx\;\;config_file:${PROJECT_SOURCE_DIR}/samples/baked/config.json\;output:baked_skeleton.ozz\;output:baked_animation.ozz"

  # Versioning
  "/fbx/alain/skeleton.fbx\;{\"skeleton\":{\"filename\":\"versioning/raw_skeleton_v1_le.ozz\",\"import\":{\"enable\":true,\"raw\":true}},\"animations\":[]}\;output:versioning/raw_skeleton_v1_le.ozz\;option:--endian=little"
  "/fbx/alain/skeleton.fbx\;{\"skeleton\":{\"filename\":\"versioning/raw_skeleton_v1_be.ozz\",\"import\":{\"enable\":true,\"raw\":true}},\"animations\":[]}\;output:versioning/raw_skeleton_v1_be.ozz\;option:--endian=big"
  "/fbx/alain/skeleton.fbx\;{\"skeleton\":{\"filename\":\"versioning/skeleton_v1_le.ozz\",\"import\":{\"enable\":true}},\"animations\":[]}\;output:versioning/skeleton_v1_le.ozz\;option:--endian=little"
  "/fbx/alain/skeleton.fbx\;{\"skeleton\":{\"filename\":\"versioning/skeleton_v1_be.ozz\",\"import\":{\"enable\":true}},\"animations\":[]}\;output:versioning/skeleton_v1_be.ozz\;option:--endian=big"
  "/fbx/alain/run.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"versioning/raw_animation_v2_le.ozz\",\"raw\":true}]}\;output:versioning/raw_animation_v2_le.ozz\;option:--endian=little\;depend:alain_skeleton.ozz"
  "/fbx/alain/run.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"versioning/raw_animation_v2_be.ozz\",\"raw\":true}]}\;output:versioning/raw_animation_v2_be.ozz\;option:--endian=big\;depend:alain_skeleton.ozz"
  "/fbx/alain/run.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"versioning/animation_v5_le.ozz\"}]}\;output:versioning/animation_v5_le.ozz\;option:--endian=little\;depend:alain_skeleton.ozz"
  "/fbx/alain/run.fbx\;{\"skeleton\":{\"filename\":\"alain_skeleton.ozz\",\"import\":{\"enable\":false}},\"animations\":[{\"filename\":\"versioning/animation_v5_be.ozz\"}]}\;output:versioning/animation_v5_be.ozz\;option:--endian=big\;depend:alain_skeleton.ozz"

  # Collada
  "/collada/astro_max.dae\;{\"skeleton\":{\"filename\":\"astro_max_skeleton.ozz\",\"import\":{\"enable\":true}},\"animations\":[{\"filename\":\"astro_max_animation.ozz\"}]}\;output:astro_max_animation.ozz\;output:astro_max_skeleton.ozz"
  "/collada/astro_maya.dae\;{\"skeleton\":{\"filename\":\"astro_maya_skeleton.ozz\",\"import\":{\"enable\":true}},\"animations\":[{\"filename\":\"astro_maya_animation.ozz\"}]}\;output:astro_maya_animation.ozz\;output:astro_maya_skeleton.ozz"
  "/collada/seymour.dae\;{\"skeleton\":{\"filename\":\"seymour_skeleton.ozz\",\"import\":{\"enable\":true}},\"animations\":[{\"filename\":\"seymour_animation.ozz\"}]}\;output:seymour_animation.ozz\;output:seymour_skeleton.ozz"
)

# Generate a command for each line of the configuration
foreach(line ${build_configurations})

  # First 3 elements are mandatory
  list(GET line 0 src)
  list(GET line 1 config)

  # Optional elements
  list(LENGTH line line_length)

  # Loops over all remaining arguments to find outputs.
  set(outputs "")
  set(options "")
  set(depends "")
  set(config_file "")
    
  # Loops over all elements to find parameters.
  foreach(element ${line})
    if (${element} MATCHES "output:(.*)")
      string(SUBSTRING ${element} 7 -1 output)
      list(APPEND outputs "${ozz_media_directory}/bin/${output}")
    endif()
    if (${element} MATCHES "depend:(.*)")
      string(SUBSTRING ${element} 7 -1 depend)
      list(APPEND depends "${ozz_media_directory}/bin/${depend}")
    endif()
    if (${element} MATCHES "option:(.*)")
      string(SUBSTRING ${element} 7 -1 option)
      list(APPEND options ${option})
    endif()
    if (${element} MATCHES "config_file:(.*)")
      string(SUBSTRING ${element} 12 -1 config_file)
    endif()
  endforeach()

  list(APPEND all_outputs ${outputs})
  # output is required
  if (NOT outputs)
    message("Missing output file(s) list.")
  endif()

  # Dump config file
  if(NOT config_file)
    string(MD5 hash "${line}")
    set(config_file "${ozz_temp_directory}/config_${hash}.json")
    file(WRITE "${config_file}" "${config}")
  else()
    list(APPEND depends "${config_file}")
  endif()

  # Create build command
  add_custom_command(
    DEPENDS "${ozz_media_directory}${src}"
            ${depends}
            $<$<AND:$<BOOL:${ozz_build_data}>,$<BOOL:${ozz_build_fbx}>>:fbx2ozz>
    OUTPUT  ${outputs}
    COMMAND fbx2ozz
            "--file=${ozz_media_directory}${src}"
            "--config_file=${config_file}"
            ${options}
    WORKING_DIRECTORY "${ozz_media_directory}/bin/"
    VERBATIM)
endforeach()

# Creates a target to build data
add_custom_target(BUILD_DATA ALL
  DEPENDS ${all_outputs}
  VERBATIM)
