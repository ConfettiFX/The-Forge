tf_add_unit_test(12_ZipFileSystem 12_ZipFileSystem.cpp)

determine_resources_dir(12_ZipFileSystem resources_dir)

add_custom_command(
    TARGET 12_ZipFileSystem
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Art/UnitTestResources/ZipFilesDds/ ${resources_dir}/ZipFiles/
    COMMENT "Deploying 12_ZipFileSystem's assets"
)
