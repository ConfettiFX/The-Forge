tf_add_unit_test(15a_VisibilityBufferOIT 15a_VisibilityBufferOIT.cpp ../../../Visibility_Buffer/src/SanMiguel.cpp)

target_link_libraries(15a_VisibilityBufferOIT ozz_base ozz_animation)

determine_resources_dir(15a_VisibilityBufferOIT resources_dir)

add_custom_command(
    TARGET 15a_VisibilityBufferOIT
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/SanMiguel_3/Textures/dds/ ${resources_dir}/Textures/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/SanMiguel_3/Meshes/ ${resources_dir}/Meshes/
    COMMAND ${CMAKE_COMMAND} -E make_directory ${resources_dir}/Meshes/stormtrooper/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/stormtrooper/riggedMesh.bin
            ${resources_dir}/Meshes/stormtrooper/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/stormtrooper/animations/
            ${resources_dir}/Animations/stormtrooper/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/stormtrooper/skeleton.ozz
            ${resources_dir}/Animations/stormtrooper/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds/Stormtrooper_D.tex ${resources_dir}/Textures/
    COMMENT "Deploying 15a_VisibilityBufferOIT's assets"
)
