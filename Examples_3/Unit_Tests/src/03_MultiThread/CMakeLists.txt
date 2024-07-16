tf_add_unit_test(03_MultiThread 03_MultiThread.cpp)

determine_resources_dir(03_MultiThread resources_dir)

add_custom_command(
    TARGET 03_MultiThread
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds ${resources_dir}/Textures
    COMMENT "Deploying 03_MultiThread's assets"
)
