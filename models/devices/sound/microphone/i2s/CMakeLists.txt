set(MIC_SRCS "i2s_microphone.cpp")

vp_model(NAME devices.sound.microphone.i2s.i2s_microphone
FORCE_BUILD 1
    SOURCES ${MIC_SRCS}
    )

vp_model_link_blocks(
    NAME devices.sound.microphone.i2s.i2s_microphone
    BLOCK io_audio
    FORCE_BUILD 1
    )
