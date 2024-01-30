set(MIC_SRCS "pdm_microphone.cpp")


vp_model(NAME devices.sound.microphone.pdm.pdm_microphone
    SOURCES ${MIC_SRCS}
    )

vp_model_link_blocks(
    NAME devices.sound.microphone.pdm.pdm_microphone
    BLOCK pcm_pdm_conversion
    )

vp_model_link_blocks(
    NAME devices.sound.microphone.pdm.pdm_microphone
    BLOCK io_audio
    )


find_library(SNDFILE_LIB sndfile)
if(SNDFILE_LIB)
    vp_model_compile_options(NAME devices.sound.microphone.pdm.pdm_microphone OPTIONS "-DUSE_SNDFILE")
    vp_model_link_libraries(
        NAME devices.sound.microphone.pdm.pdm_microphone NO_M32 1 LIBRARY sndfile)        
endif()
         
