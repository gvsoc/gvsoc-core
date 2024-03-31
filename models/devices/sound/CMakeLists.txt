add_subdirectory(dac)
add_subdirectory(utils)
add_subdirectory(microphone)

vp_model(NAME devices.sound.i2s_microphone
         SOURCES "i2s_microphone.cpp")

vp_model(NAME devices.sound.i2s_speaker
         SOURCES "i2s_speaker.cpp")

vp_model(NAME devices.sound.i2s_clock
         SOURCES "i2s_clock.cpp")

find_library(SNDFILE_LIB sndfile)
if(SNDFILE_LIB)
    vp_model_compile_options(NAME devices.sound.i2s_speaker OPTIONS "-DUSE_SNDFILE")
    vp_model_link_libraries(NAME devices.sound.i2s_speaker NO_M32 1 LIBRARY sndfile)

    vp_model_compile_options(NAME devices.sound.i2s_microphone OPTIONS "-DUSE_SNDFILE")
    vp_model_link_libraries(NAME devices.sound.i2s_microphone NO_M32 1 LIBRARY sndfile)
endif()
