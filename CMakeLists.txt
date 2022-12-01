cmake_minimum_required(VERSION 3.16.3)

# GVSOC needs to be given a config file containing all the components to be compiled

foreach(subdir ${GVSOC_MODULES})
    set(GAPY_TARGETS_DIR_OPT ${GAPY_TARGETS_DIR_OPT} --target-dir=${subdir}/models)
endforeach()

foreach(target ${GVSOC_TARGETS})
    set(GAPY_TARGETS_OPT ${GAPY_TARGETS_OPT} --target=${target})
    if(NOT DEFINED GAPY_TARGETS_NAME)
        set(GAPY_TARGETS_NAME ${target})
    else()
        set(GAPY_TARGETS_NAME ${GAPY_TARGETS_NAME}_${target})
    endif()
endforeach()


set(TARGET_CONFIG_DIR ${CMAKE_BINARY_DIR}/gvsoc/configs)
set(TARGET_CONFIG ${TARGET_CONFIG_DIR}/${GAPY_TARGETS_NAME}.config)
file(MAKE_DIRECTORY ${TARGET_CONFIG_DIR})

message(STATUS "Generating GVSOC config to ${TARGET_CONFIG} with command: ")

# Gapy is used to extract the list of components out of the specified target
set(GAPY_CMD gapy ${GAPY_TARGETS_OPT} ${GAPY_TARGETS_DIR_OPT}
    --platform=gvsoc
    --component-file=${TARGET_CONFIG} components
    --py-stack
)

string(REPLACE ";"  " " GAPY_CMD_STR "    ${GAPY_CMD}")
message(STATUS ${GAPY_CMD_STR})

# We generate first the list of components for the specified target
execute_process (
    COMMAND ${GAPY_CMD} RESULT_VARIABLE ret
)
if(ret EQUAL "1")
    message( FATAL_ERROR "Caught error while generating gvsoc config")
endif()


project(gvsoc-core)

# 2.2 Misc
#
# import_kconfig(<prefix> <kconfig_fragment> [<keys>])
#
# Parse a KConfig fragment (typically with extension .config) and
# introduce all the symbols that are prefixed with 'prefix' into the
# CMake namespace. List all created variable names in the 'keys'
# output variable if present.
function(import_kconfig prefix kconfig_fragment)
  # Parse the lines prefixed with 'prefix' in ${kconfig_fragment}
  file(
    STRINGS
    ${kconfig_fragment}
    DOT_CONFIG_LIST
    REGEX "^${prefix}"
    ENCODING "UTF-8"
  )

  foreach (CONFIG ${DOT_CONFIG_LIST})
    # CONFIG could look like: CONFIG_NET_BUF=y

    # Match the first part, the variable name
    string(REGEX MATCH "[^=]+" CONF_VARIABLE_NAME ${CONFIG})

    # Match the second part, variable value
    string(REGEX MATCH "=(.+$)" CONF_VARIABLE_VALUE ${CONFIG})
    # The variable name match we just did included the '=' symbol. To just get the
    # part on the RHS we use match group 1
    set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})

    if("${CONF_VARIABLE_VALUE}" MATCHES "^\"(.*)\"$") # Is surrounded by quotes
      set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})
    endif()

    set("${CONF_VARIABLE_NAME}" "${CONF_VARIABLE_VALUE}" PARENT_SCOPE)
    list(APPEND keys "${CONF_VARIABLE_NAME}")
  endforeach()

  foreach(outvar ${ARGN})
    set(${outvar} "${keys}" PARENT_SCOPE)
  endforeach()
endfunction()

if(DEFINED TARGET_CONFIG)
import_kconfig(CONFIG_ ${TARGET_CONFIG})
else()
set(CONFIG_BUILD_ALL 1)
endif()


# install sub folder
set(GVSOC_MODELS_INSTALL_FOLDER       "python")
set(GVSOC_MODELS_OPTIM_INSTALL_FOLDER "")
set(GVSOC_MODELS_DEBUG_INSTALL_FOLDER "debug")
set(GVSOC_MODELS_SV_INSTALL_FOLDER    "sv")
set(GVSOC_MODELS_OPTIM_M32_INSTALL_FOLDER "m32")
set(GVSOC_MODELS_DEBUG_M32_INSTALL_FOLDER "debug_m32")

# ================
# Utility includes
# ================
include(cmake/vp_model.cmake)

# =======
# Options
# =======
option(BUILD_OPTIMIZED     "build GVSOC with optimizations"                    ON)
option(BUILD_DEBUG         "build GVSOC with debug information"                ON)
option(BUILD_RTL           "build GVSOC for RTL simulation optimizations"      ON)
option(BUILD_OPTIMIZED_M32 "build GVSOC with optimizations in 32bits mode"     OFF)
option(BUILD_DEBUG_M32     "build GVSOC with debug information in 32bits mode" OFF)
option(SKIP_DPI "Do not build DPI" OFF)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3")
set(CMAKE_CC_FLAGS_RELWITHDEBINFO "-g -O3")


# set target types
vp_set_target_types(
    BUILD_DEBUG ${BUILD_DEBUG}
    BUILD_OPTIMIZED ${BUILD_OPTIMIZED}
    BUILD_RTL ${BUILD_RTL}
    BUILD_OPTIMIZED_M32 ${BUILD_OPTIMIZED_M32}
    BUILD_DEBUG_M32 ${BUILD_DEBUG_M32}
    )

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

install(DIRECTORY bin/ DESTINATION bin USE_SOURCE_PERMISSIONS)

add_subdirectory(dpi-wrapper)
add_subdirectory(engine)
add_subdirectory(launcher)
add_subdirectory(models)

foreach(subdir ${GVSOC_MODULES})
    if(IS_DIRECTORY ${subdir})
        if (NOT ${subdir} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
            get_filename_component(name ${subdir} NAME)
            add_subdirectory(${subdir} ${CMAKE_CURRENT_BINARY_DIR}/${name})
        endif()
    endif()
endforeach()
