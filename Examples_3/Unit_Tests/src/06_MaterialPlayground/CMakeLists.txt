tf_add_unit_test(06_MaterialPlayground 06_MaterialPlayground.cpp)

target_link_libraries(06_MaterialPlayground ozz_base ozz_animation)
determine_resources_dir(06_MaterialPlayground resources_dir)

add_custom_command(
    TARGET 06_MaterialPlayground
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Animation/ ${resources_dir}/Animation
    COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Common_3/Tools/ForgeMaterialCompiler/forge_material_compiler.py -d
            ${CMAKE_CURRENT_SOURCE_DIR}/Materials -o ${resources_dir}/CompiledMaterials/ --verbose
    COMMAND ${CMAKE_COMMAND} -E make_directory ${resources_dir}/Textures/PBR
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/PBR/dds/ ${resources_dir}/Textures/PBR
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Textures/dds/LA_Helipad3D.tex ${resources_dir}/Textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Meshes ${resources_dir}/Meshes
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/Hair ${resources_dir}/Meshes/Hair
    COMMENT "Deploying 06_MaterialPlayground's assets"
)
