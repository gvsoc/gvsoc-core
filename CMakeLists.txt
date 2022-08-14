cmake_minimum_required(VERSION 3.16.3)

project(gvsoc-core)

foreach(subdir ${VP_MODEL_ROOT_DIRS})
    if(IS_DIRECTORY ${subdir})
    endif()
endforeach()

# install sub folder
set(GVSOC_MODELS_INSTALL_FOLDER       "python")
set(GVSOC_MODELS_OPTIM_INSTALL_FOLDER "")
set(GVSOC_MODELS_DEBUG_INSTALL_FOLDER "debug")
set(GVSOC_MODELS_SV_INSTALL_FOLDER    "sv")

# ================
# Utility includes
# ================
include(cmake/vp_model.cmake)

# =======
# Options
# =======
option(BUILD_OPTIMIZED "build GVSOC with optimizations"               ON)
option(BUILD_DEBUG     "build GVSOC with debug information"           ON)
option(BUILD_RTL       "build GVSOC for RTL simulation optimizations" ON)
option(SKIP_DPI "Do not build DPI" OFF)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3")
set(CMAKE_CC_FLAGS_RELWITHDEBINFO "-g -O3")

# verify supported chip
set(SUPPORTED_TARGET_CHIPS
    "GAP8"
    "GAP8_V2"
    "GAP8_V3"
    "GAP9"
    "GAP9_V2"
    "GAP9_5"
    )
if(NOT "${TARGET_CHIP}" IN_LIST SUPPORTED_TARGET_CHIPS)
    message(FATAL_ERROR "${TARGET_CHIP} chip is not supported by GVSOC")
endif()

# set target types
vp_set_target_types(
    BUILD_DEBUG ${BUILD_DEBUG}
    BUILD_OPTIMIZED ${BUILD_OPTIMIZED}
    BUILD_RTL ${BUILD_RTL}
    )

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

install(DIRECTORY bin/ DESTINATION bin USE_SOURCE_PERMISSIONS)

add_subdirectory(dpi-wrapper)
add_subdirectory(engine)
add_subdirectory(launcher)
add_subdirectory(models)

foreach(subdir ${VP_MODEL_ROOT_DIRS})
    if(IS_DIRECTORY ${subdir})
        if (NOT ${subdir} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
            get_filename_component(name ${subdir} NAME)
            add_subdirectory(${subdir} ${CMAKE_CURRENT_BINARY_DIR}/${name})
        endif()
    endif()
endforeach()
