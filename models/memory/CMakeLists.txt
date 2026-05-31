vp_model(NAME memory.dramsys
    SOURCES "dramsys.cpp"
    )

vp_model_include_directories(
    NAME memory.dramsys
    DIRECTORY "include"
    )

vp_model(NAME memory.dramsys_v2
    SOURCES "dramsys_v2.cpp"
    )

vp_model_include_directories(
    NAME memory.dramsys_v2
    DIRECTORY "include"
    )

vp_model(NAME memory.pim_component
    SOURCES "pim_component.cpp"
    )

vp_model_include_directories(
    NAME memory.pim_component
    DIRECTORY "include"
    )

set_source_files_properties(dramsys.cpp dramsys_v2.cpp PROPERTIES
    COMPILE_DEFINITIONS "DRAMSYS_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
