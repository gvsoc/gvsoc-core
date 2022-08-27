cmake_minimum_required(VERSION 3.16.3)

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
