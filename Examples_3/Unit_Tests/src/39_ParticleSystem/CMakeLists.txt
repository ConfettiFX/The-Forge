tf_add_unit_test(39_ParticleSystem 39_ParticleSystem.cpp ../../../Visibility_Buffer/src/SanMiguel.cpp)

determine_resources_dir(39_ParticleSystem resources_dir)

add_custom_command(
    TARGET 39_ParticleSystem
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/SanMiguel_3/Textures/dds/ ${resources_dir}/Textures/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/Particles/Textures/dds/ ${resources_dir}/Textures/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/SanMiguel_3/Meshes/ ${resources_dir}/Meshes/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/cameraPath.bin ${resources_dir}/
    COMMENT "Deploying 39_ParticleSystem's assets"
)
