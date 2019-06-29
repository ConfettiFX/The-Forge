#add_subdirectory(${PROJECT_SOURCE_DIR}/extern/jsoncpp json)

add_library(ozz_animation_tools STATIC
  ${PROJECT_SOURCE_DIR}/include/ozz/animation/offline/tools/import2ozz.h
  import2ozz.cc
  import2ozz_anim.h
  import2ozz_anim.cc
  import2ozz_config.h
  import2ozz_config.cc
  import2ozz_skel.h
  import2ozz_skel.cc)
target_link_libraries(ozz_animation_tools
  ozz_animation_offline
  ozz_options
  json)
set_target_properties(ozz_animation_tools
  PROPERTIES FOLDER "ozz/tools")

install(TARGETS ozz_animation_tools DESTINATION lib)

fuse_target("ozz_animation_tools")

if(NOT EMSCRIPTEN)
  add_executable(dump2ozz
    dump2ozz.cc)
  target_link_libraries(dump2ozz
    ozz_animation_tools
    ozz_options)

  # Uses a fake import file to dump reference and default configurations.
  add_custom_command(TARGET dump2ozz POST_BUILD
    COMMAND dump2ozz "--file=${CMAKE_CURRENT_SOURCE_DIR}/dump2ozz.cc" "--config={\"skeleton\": {\"import\":{\"enable\":false}},\"animations\":[]}" "--config_dump_reference=${CMAKE_CURRENT_SOURCE_DIR}/reference.json" VERBATIM)

  set_target_properties(dump2ozz
    PROPERTIES FOLDER "ozz/tools")
endif()