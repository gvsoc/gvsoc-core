set(SRCS "io_audio.cpp")


vp_block(NAME io_audio
    PREFIX io_audio
    SOURCES ${SRCS})

find_library(SNDFILE_LIB sndfile)
find_path(SNDFILE_INC sndfile.hh)
find_library(SAMPLERATE_LIB samplerate)
find_path(SAMPLERATE_INC samplerate.h)

if(SNDFILE_LIB AND SNDFILE_INC)
    list(APPEND COMPILE_OPTIONS "-DUSE_SNDFILE" "-I${SNDFILE_INC}")
    list(APPEND LIBS_TO_INCLUDE ${SNDFILE_LIB})
endif()

if(SAMPLERATE_LIB AND SAMPLERATE_INC)
    list(APPEND COMPILE_OPTIONS "-DUSE_SAMPLERATE" "-I${SAMPLERATE_INC}")
    list(APPEND LIBS_TO_INCLUDE ${SAMPLERATE_LIB})
endif()

if(SNDFILE_LIB OR SAMPLERATE_LIB)
    vp_block_compile_options(NAME io_audio OPTIONS ${COMPILE_OPTIONS})
    vp_block_link_libraries(
        NAME io_audio NO_M32 1 LIBRARY ${LIBS_TO_INCLUDE})
endif()
