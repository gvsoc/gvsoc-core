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
         
