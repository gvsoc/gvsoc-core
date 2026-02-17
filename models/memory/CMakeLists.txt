vp_model(NAME memory.dramsys
    SOURCES "dramsys.cpp"
    )

vp_model_include_directories(
    NAME memory.dramsys
    DIRECTORY "include"
    )

vp_model(NAME memory.pim_component
    SOURCES "pim_component.cpp"
    )

vp_model_include_directories(
    NAME memory.pim_component
    DIRECTORY "include"
    )

set_source_files_properties(dramsys.cpp PROPERTIES COMPILE_DEFINITIONS "DRAMSYS_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
