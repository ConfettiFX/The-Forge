tf_add_unit_test(28_Skinning 28_Skinning.cpp)

target_link_libraries(28_Skinning ozz_base ozz_animation)

determine_resources_dir(28_Skinning resources_dir)

add_custom_command(
    TARGET 28_Skinning
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${resources_dir}/Textures
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds/Stormtrooper_D.tex ${resources_dir}/Textures/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/ ${resources_dir}/Animation/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/ ${resources_dir}/Meshes/
    COMMENT "Deploying 28_Skinning's assets"
)
