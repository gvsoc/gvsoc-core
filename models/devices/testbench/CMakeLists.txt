
set(TESTBENCH_SRCS "i2s_verif.cpp"
    "spim_verif.cpp"
    "testbench.cpp"
    )

vp_model(NAME devices.testbench.testbench
    FORCE_BUILD 1
    SOURCES ${TESTBENCH_SRCS}
    )

vp_model_link_blocks(
    NAME devices.testbench.testbench
    FORCE_BUILD 1
    BLOCK pcm_pdm_conversion
    )

find_library(SNDFILE_LIB sndfile)
if(SNDFILE_LIB)
    vp_model_compile_options(NAME devices.testbench.testbench FORCE_BUILD 1 OPTIONS "-DUSE_SNDFILE")
    vp_model_link_libraries(
        NAME devices.testbench.testbench NO_M32 1 FORCE_BUILD 1 LIBRARY sndfile)
        
endif()

