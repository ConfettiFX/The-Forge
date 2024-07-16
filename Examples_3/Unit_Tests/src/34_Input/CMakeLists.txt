tf_add_unit_test(34_Input 34_Input.cpp)

determine_resources_dir(34_Input resources_dir)

add_custom_command(
    TARGET 34_Input
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds/input/ ${resources_dir}/Textures/
    COMMENT "Deploying 34_Input's assets"
)
