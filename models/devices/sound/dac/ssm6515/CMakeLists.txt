vp_model(NAME devices.sound.dac.ssm6515.ssm6515
    SOURCES "ssm6515.cpp"
    )

vp_model_link_blocks(
    NAME devices.sound.dac.ssm6515.ssm6515
    BLOCK pcm_pdm_conversion
    )

find_library(SNDFILE_LIB sndfile)
if(SNDFILE_LIB)
    vp_model_compile_options(NAME devices.sound.dac.ssm6515.ssm6515 OPTIONS "-DUSE_SNDFILE")
    vp_model_link_libraries(
        NAME devices.sound.dac.ssm6515.ssm6515 NO_M32 1 LIBRARY sndfile)
        
endif()
         
