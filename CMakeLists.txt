cmake_minimum_required(VERSION 3.16.3)

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

# GVSOC needs to be given a config file containing all the components to be compiled

foreach(subdir ${GVSOC_MODULES})
    set(GAPY_TARGETS_DIR_OPT ${GAPY_TARGETS_DIR_OPT} --target-dir=${subdir}/models)
endforeach()

set(TARGET_CONFIG_DIR ${CMAKE_BINARY_DIR}/gvsoc/configs)
file(MAKE_DIRECTORY ${TARGET_CONFIG_DIR})

if(DEFINED GVSOC_TARGETS)

    foreach(target ${GVSOC_TARGETS})

        set(TARGET_CONFIG ${TARGET_CONFIG_DIR}/${target}.config)

        message(STATUS "Generating GVSOC config to ${TARGET_CONFIG} with command: ")

        # Gapy is used to extract the list of components out of the specified target
        set(GAPY_CMD gapy --target=${target} ${GAPY_TARGETS_DIR_OPT}
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
        
        import_kconfig(CONFIG_ ${TARGET_CONFIG})
        
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
option(BUILD_RTL           "build GVSOC for RTL simulation optimizations"      OFF)
option(BUILD_OPTIMIZED_M32 "build GVSOC with optimizations in 32bits mode"     OFF)
option(BUILD_DEBUG_M32     "build GVSOC with debug information in 32bits mode" OFF)
option(SKIP_DPI "Do not build DPI" OFF)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2")
set(CMAKE_CC_FLAGS_RELWITHDEBINFO "-g -O2")


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

if(${BUILD_RTL})
  add_subdirectory(dpi-wrapper)
endif()
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
