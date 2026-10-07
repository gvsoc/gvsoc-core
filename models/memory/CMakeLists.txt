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

# Ramulator 2.x timing model. Built only when a Ramulator install is available,
# located via the RAMULATOR_HOME env var (a dir containing libramulator.so and
# src/ headers — see the build instructions). Like DRAMSYS_PATH for dramsys.
# Ramulator exposes a C++ API (not a C ABI), so it is linked directly and the
# model is compiled with -std=c++20.
if(DEFINED ENV{RAMULATOR_HOME})
    set(RAMULATOR_DIR $ENV{RAMULATOR_HOME})
    find_library(RAMULATOR_LIB ramulator
        PATHS ${RAMULATOR_DIR} ${RAMULATOR_DIR}/build
        NO_DEFAULT_PATH)
endif()
if(RAMULATOR_LIB)
    message(STATUS "Ramulator found (${RAMULATOR_LIB}); building memory.ramulator")
    # ramulator_bridge.cpp must be a direct source (not an archive) so its
    # GvsocBridge plugin's static RAMULATOR_REGISTER_IMPLEMENTATION registrar is
    # retained in the .so and self-registers with Ramulator's Factory at load.
    vp_model(NAME memory.ramulator
        SOURCES "ramulator.cpp" "ramulator_bridge.cpp"
        )
    vp_model_include_directories(NAME memory.ramulator
        DIRECTORY "${RAMULATOR_DIR}/src"
        )
    vp_model_compile_options(NAME memory.ramulator
        OPTIONS "-std=c++20"
        )
    # Link by full path so the loader records the library directory; also add
    # $RAMULATOR_HOME to LD_LIBRARY_PATH at runtime if the .so is not found.
    vp_model_link_libraries(NAME memory.ramulator NO_M32 1 LIBRARY ${RAMULATOR_LIB})
else()
    message(STATUS "Ramulator not found (set RAMULATOR_HOME); skipping memory.ramulator")
endif()
