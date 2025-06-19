set(SRCS "io_audio.cpp")


vp_block(NAME io_audio
    PREFIX io_audio
    SOURCES ${SRCS})

find_library(SNDFILE_LIB sndfile)
find_library(SAMPLERATE_LIB samplerate)

if(SNDFILE_LIB)
    list(APPEND COMPILE_OPTIONS "-DUSE_SNDFILE")
    list(APPEND LIBS_TO_INCLUDE ${SNDFILE_LIB})
endif()

if(SAMPLERATE_LIB)
    list(APPEND COMPILE_OPTIONS "-DUSE_SAMPLERATE")
    list(APPEND LIBS_TO_INCLUDE ${SAMPLERATE_LIB})
endif()

if(SNDFILE_LIB OR SAMPLERATE_LIB)
    vp_block_compile_options(NAME io_audio OPTIONS ${COMPILE_OPTIONS})
    vp_block_link_libraries(
        NAME io_audio NO_M32 1 LIBRARY ${LIBS_TO_INCLUDE})
endif()
