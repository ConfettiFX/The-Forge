tf_add_unit_test(01_Transformations 01_Transformations.cpp)

determine_resources_dir(01_Transformations resources_dir)

add_custom_command(
    TARGET 01_Transformations
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds ${resources_dir}/Textures
    COMMENT "Deploying 01_Transformation's assets"
)
