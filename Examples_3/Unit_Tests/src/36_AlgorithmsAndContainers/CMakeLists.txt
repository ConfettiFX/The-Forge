tf_add_unit_test(36_AlgorithmsAndContainers 36_AlgorithmsAndContainers.cpp AlgorithmsTest.c)

if(APPLE)
    set_source_files_properties(AlgorithmsTest.c PROPERTIES COMPILE_FLAGS "-x c")
endif()
