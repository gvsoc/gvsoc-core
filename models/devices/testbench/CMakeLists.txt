
set(TESTBENCH_SRCS "i2s_verif.cpp"
    "spim_verif.cpp"
    "testbench.cpp"

    "pcm_pdm_conversion/sigma_delta.cpp"
    "pcm_pdm_conversion/sigma_delta_arithmetics.cpp"
    "pcm_pdm_conversion/linear_interpolator.cpp"
    "pcm_pdm_conversion/iir_interpolator.cpp"
    "pcm_pdm_conversion/pcm_pdm_conversion.cpp"
    )

vp_model(NAME devices.testbench.testbench
    FORCE_BUILD 1
    SOURCES ${TESTBENCH_SRCS}
    )

find_library(SNDFILE_LIB sndfile)
if(SNDFILE_LIB)
    vp_model_compile_options(NAME devices.testbench.testbench FORCE_BUILD 1 OPTIONS "-DUSE_SNDFILE")
    vp_model_link_libraries(
        NAME devices.testbench.testbench NO_M32 1 FORCE_BUILD 1 LIBRARY sndfile)
        
endif()

