vp_model(NAME devices.camera.himax
    SOURCES "himax.cpp")

if(NOT DEFINED ENV{NO_IMAGE_MAGICK})
find_library(MAGICK_LIB GraphicsMagick++)
if(MAGICK_LIB)
    vp_model_compile_options(NAME devices.camera.himax OPTIONS "-D__MAGICK__")
    vp_model_link_libraries(NAME devices.camera.himax NO_M32 1 LIBRARY GraphicsMagick++)
else()
endif()
endif()
