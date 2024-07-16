tf_add_unit_test(17_EntityComponentSystem 17_EntityComponentSystem.cpp)

determine_resources_dir(17_EntityComponentSystem resources_dir)

add_custom_command(
    TARGET 17_EntityComponentSystem
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${resources_dir}/Textures/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds/sprites.tex ${resources_dir}/Textures/
    COMMENT "Deploying 17_EntityComponentSystem's assets"
)
