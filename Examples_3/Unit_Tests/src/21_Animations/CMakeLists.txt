tf_add_unit_test(21_Animations 21_Animations.cpp)

target_link_libraries(21_Animations ozz_animation)

determine_resources_dir(21_Animations resources_dir)

add_custom_command(
    TARGET 21_Animations
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/ ${resources_dir}/Animation/
    COMMENT "Deploying 21_Animations's assets"
)
