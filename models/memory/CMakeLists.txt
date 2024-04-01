vp_model(NAME memory.dramsys
    SOURCES "dramsys.cpp"
    )

set_source_files_properties(dramsys.cpp PROPERTIES COMPILE_DEFINITIONS "DRAMSYS_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\"")