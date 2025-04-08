cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_INSTALL_MESSAGE LAZY)

if (NOT CMAKE_SKIP_INSTALL_RPATH)

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

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
function(import_kconfig prefix kconfig_fragment list_name)
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

    if("${CONF_VARIABLE_NAME}" STREQUAL "CONFIG_COMPONENTS")
        string(REPLACE " "  ";" COMPONENT_LIST "${CONF_VARIABLE_VALUE}")
        foreach (COMPONENT ${COMPONENT_LIST})
            list(FIND ${list_name} "${COMPONENT}" INDEX)

            # Check if "Orange" was not found and append it
            if(INDEX EQUAL -1)
                list(APPEND ${list_name} "${COMPONENT}")
            endif()

            set(${list_name} "${${list_name}}" PARENT_SCOPE)
        endforeach()
    else()
        set("${CONF_VARIABLE_NAME}" "${CONF_VARIABLE_VALUE}" PARENT_SCOPE)
    endif()

    list(APPEND keys "${CONF_VARIABLE_NAME}")
  endforeach()

  foreach(outvar ${ARGN})
    set(${outvar} "${keys}" PARENT_SCOPE)
  endforeach()
endfunction()

# GVSOC needs to be given a config file containing all the components to be compiled

foreach(subdir ${GVSOC_MODULES})
    set(GAPY_TARGETS_DIR_OPT ${GAPY_TARGETS_DIR_OPT} --target-dir=${subdir})
endforeach()

set(TARGET_CONFIG_DIR ${CMAKE_BINARY_DIR}/gvsoc/configs)
file(MAKE_DIRECTORY ${TARGET_CONFIG_DIR})

set(COMPONENTS)

if(DEFINED GVSOC_TARGETS)

    string(REPLACE " "  ";" GVSOC_TARGETS_LIST "${GVSOC_TARGETS}")
    foreach(target ${GVSOC_TARGETS_LIST})

        string(REPLACE ","  "_" TARGET_NAME "${target}")
        string(REPLACE "/"  "." TARGET_NAME "${TARGET_NAME}")
        string(REPLACE ":"  "_" TARGET_NAME "${TARGET_NAME}")
        set(TARGET_CONFIG ${TARGET_CONFIG_DIR}/${TARGET_NAME}.config)

        message(STATUS "Generating GVSOC config to ${TARGET_CONFIG} with command: ")

        # Gapy is used to extract the list of components out of the specified target

        if(DEFINED ENV{USE_GVRUN})
            set(GAPY_CMD gvrun --target=${target} ${GAPY_TARGETS_DIR_OPT}
                --platform=gvsoc
                --builddir=${CMAKE_CURRENT_BINARY_DIR}
                --installdir=${CMAKE_INSTALL_PREFIX}/models
                --component-file=${TARGET_CONFIG} components
                --py-stack
            )
        else()
            set(GAPY_CMD gapy --target=${target} ${GAPY_TARGETS_DIR_OPT}
                --platform=gvsoc
                --builddir=${CMAKE_CURRENT_BINARY_DIR}
                --installdir=${CMAKE_INSTALL_PREFIX}/models
                --component-file=${TARGET_CONFIG} components
                --py-stack
            )
        endif()

        string(REPLACE ";"  " " GAPY_CMD_STR "    ${GAPY_CMD}")
        message(STATUS ${GAPY_CMD_STR})

        # We generate first the list of components for the specified target
        execute_process (
            COMMAND ${GAPY_CMD} RESULT_VARIABLE ret
        )
        if(NOT ret EQUAL "0")
            message( FATAL_ERROR "Caught error while generating gvsoc config")
        endif()

        import_kconfig(CONFIG_ ${TARGET_CONFIG} "COMPONENTS")

    endforeach()

else()
    set(CONFIG_BUILD_ALL 1)
endif()

# install sub folder
set(GVSOC_MODELS_INSTALL_FOLDER       "models")
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
option(BUILD_OPTIMIZED_M32 "build GVSOC with optimizations in 32bits mode"     OFF)
option(BUILD_DEBUG_M32     "build GVSOC with debug information in 32bits mode" OFF)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3 -std=c++17")
set(CMAKE_CC_FLAGS_RELWITHDEBINFO "-g -O3")


# set target types
vp_set_target_types(
    BUILD_DEBUG ${BUILD_DEBUG}
    BUILD_OPTIMIZED ${BUILD_OPTIMIZED}
    BUILD_OPTIMIZED_M32 ${BUILD_OPTIMIZED_M32}
    BUILD_DEBUG_M32 ${BUILD_DEBUG_M32}
    )

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

install(DIRECTORY bin/ DESTINATION bin USE_SOURCE_PERMISSIONS)

if (NOT APPLE)
    # SCHEREMO: DPI is not supported on anything but Linux anyway / no EDA tools on Apple
    add_subdirectory(dpi-wrapper)
endif()
add_subdirectory(engine)
add_subdirectory(models)

set(is_first true)

foreach(subdir ${GVSOC_MODULES})

    if (NOT ${is_first})
        if(IS_DIRECTORY ${subdir})
            get_filename_component(name ${subdir} NAME)
            message(STATUS ${subdir})
            if(EXISTS ${subdir}/CMakeLists.txt)
                add_subdirectory(${subdir} ${CMAKE_CURRENT_BINARY_DIR}/${name}/models)
            endif()
        endif()
    endif()

    set(is_first false)
endforeach()

foreach(component ${COMPONENTS})
    string(REPLACE " " ";" SRCS_LIST "${CONFIG_SRCS_${component}}")
    vp_model(NAME ${component}
        SOURCES "${SRCS_LIST}"
    )
endforeach()
