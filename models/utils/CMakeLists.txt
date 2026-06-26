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

# ---------------------------------------------------------------------------
# IoV2BeatAdapter — standalone component auto-inserted by the gvrun2 systree
# on io_v2 bindings where the master signature is IoV2Beat and the slave
# signature is IoV2BigPacket. Normalises any of the three v2 slave response
# forms (sync DONE, async big-packet, beat stream) into a uniform per-beat
# resp() stream observed by the upstream master.
# ---------------------------------------------------------------------------
vp_model(NAME utils.io_v2_beat_adapter
    SOURCES "io_v2_beat_adapter.cpp"
)

vp_model(NAME utils.io_v2_beat_collapse_adapter
    SOURCES "io_v2_beat_collapse_adapter.cpp"
)

# ---------------------------------------------------------------------------
# IoV2BeatSyncAdapter — dedicated adapter for the IoV2Beat master <-> IoV2Sync
# slave case. The sync slave always replies IO_REQ_DONE inline, so this drops
# the general adapter's per-beat sub-read pipeline and just spreads the inline
# response into a per-beat resp() stream.
# ---------------------------------------------------------------------------
vp_model(NAME utils.io_v2_beat_sync_adapter
    SOURCES "io_v2_beat_sync_adapter.cpp"
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

# ---------------------------------------------------------------------------
# FST dumper (mirrors vcd_dumper for FST files). Uses libfst vendored from
# Verilator's pre-portabilized copy of GTKWave under ./fst/, so it builds out
# of the box -- no env var, no system dependency beyond zlib (which the rest
# of the gvsoc tree already needs). FORCE_BUILD 1 mirrors the FSDB rules so
# the dumper is available on per-target kconfig builds.
# ---------------------------------------------------------------------------
vp_block(NAME utils.fst_libfst
    SOURCES "fst/fstapi.c" "fst/fastlz.c" "fst/lz4.c"
)
# Vendored third-party C: silence warnings the project would otherwise turn
# into errors. Scoped to the block so the dumper itself stays warning-clean.
vp_block_compile_options(NAME utils.fst_libfst OPTIONS
    "-w" "-I${CMAKE_CURRENT_SOURCE_DIR}/fst"
)
vp_block_link_libraries(NAME utils.fst_libfst LIBRARY z)

vp_model(NAME utils.fst_dumper
    FORCE_BUILD 1
    SOURCES "fst_dumper.cpp"
)
vp_model_include_directories(NAME utils.fst_dumper
    FORCE_BUILD 1
    DIRECTORY "fst"
)
vp_model_link_blocks(NAME utils.fst_dumper
    FORCE_BUILD 1
    BLOCK utils.fst_libfst
)
