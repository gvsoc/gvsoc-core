vp_model(NAME utils.loader_impl
    SOURCES "loader_impl.cpp"
    )

vp_model_link_libraries(NAME utils.injector_impl LIBRARY gvsoc_launcher_lib)

vp_model(NAME utils.composite_impl
    SOURCES "composite_impl.cpp"
    )

vp_model(NAME utils.clock_impl
    SOURCES "clock_impl.cpp"
    )

vp_model(NAME utils.dpi_chip_wrapper
    FORCE_BUILD 1
    SOURCES "dpi_chip_wrapper.cpp"
    )

add_subdirectory(loader)

# ---------------------------------------------------------------------------
# Optional FSDB dumper (mirrors vcd_dumper for FSDB files). Enabled when
# FSDB_HOME points at an FsdbReader kit containing ffrAPI.h and the
# libnffr / libnsys shared libraries under LINUX64/. Disabled otherwise.
# ---------------------------------------------------------------------------
find_path(FSDB_INCLUDE_DIR NAMES ffrAPI.h
    HINTS $ENV{FSDB_HOME}
    NO_DEFAULT_PATH)
find_library(FSDB_NFFR_LIB NAMES nffr
    HINTS $ENV{FSDB_HOME}/LINUX64
    NO_DEFAULT_PATH)
find_library(FSDB_NSYS_LIB NAMES nsys
    HINTS $ENV{FSDB_HOME}/LINUX64
    NO_DEFAULT_PATH)

if(FSDB_INCLUDE_DIR AND FSDB_NFFR_LIB AND FSDB_NSYS_LIB)
    # FORCE_BUILD so the dumper is available on targets that use per-target
    # kconfig (CONFIG_BUILD_ALL is only set in the "build-everything" fallback,
    # and fsdb_dumper is a cross-target utility not listed in target configs).
    # Every vp_model_* helper re-checks the build gate independently, so
    # FORCE_BUILD 1 has to be passed to each one.
    vp_model(NAME utils.fsdb_dumper
        FORCE_BUILD 1
        SOURCES "fsdb_dumper.cpp"
        )
    vp_model_include_directories(NAME utils.fsdb_dumper
        FORCE_BUILD 1
        DIRECTORY "${FSDB_INCLUDE_DIR}"
        )
    vp_model_link_libraries(NAME utils.fsdb_dumper
        FORCE_BUILD 1
        NO_M32 1
        LIBRARY "${FSDB_NFFR_LIB}"
        )
    vp_model_link_libraries(NAME utils.fsdb_dumper
        FORCE_BUILD 1
        NO_M32 1
        LIBRARY "${FSDB_NSYS_LIB}"
        )
    get_filename_component(FSDB_LIB_DIR "${FSDB_NFFR_LIB}" DIRECTORY)
    vp_model_link_options(NAME utils.fsdb_dumper
        FORCE_BUILD 1
        OPTIONS "-Wl,-rpath,${FSDB_LIB_DIR}"
        )
endif()
