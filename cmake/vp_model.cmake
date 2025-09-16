if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

set(VP_TARGET_TYPES "" CACHE INTERNAL "contains the types of target")

function(vp_set_target_types)
    cmake_parse_arguments(
        VP_TARGET_TYPES
        ""
        "BUILD_OPTIMIZED;BUILD_DEBUG"
        ""
        ${ARGN}
        )
    if(${BUILD_OPTIMIZED} AND NOT "_optim" IN_LIST VP_TARGET_TYPES)
        message(STATUS "setting optimized")
        set(VP_TARGET_TYPES ${VP_TARGET_TYPES} "_optim" CACHE INTERNAL "")
    endif()
    if(${BUILD_DEBUG} AND NOT "_debug" IN_LIST VP_TARGET_TYPES)
        set(VP_TARGET_TYPES ${VP_TARGET_TYPES} "_debug" CACHE INTERNAL "")
    endif()
    if(${BUILD_OPTIMIZED_M32} AND NOT "_optim_m32" IN_LIST VP_TARGET_TYPES)
        message(STATUS "setting optimized m32")
        set(VP_TARGET_TYPES ${VP_TARGET_TYPES} "_optim_m32" CACHE INTERNAL "")
    endif()
    if(${BUILD_DEBUG_M32} AND NOT "_debug_m32" IN_LIST VP_TARGET_TYPES)
        set(VP_TARGET_TYPES ${VP_TARGET_TYPES} "_debug_m32" CACHE INTERNAL "")
    endif()
endfunction()

# vp_block function
function(vp_block)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;OUTPUT_NAME"
        "SOURCES;INCLUDES"
        ${ARGN}
        )
    #message(STATUS "vp_model: name=\"${VP_MODEL_NAME}\", output_name=\"${VP_MODEL_OUTPUT_NAME}\" prefix=\"${VP_MODEL_PREFIX}\", srcs=\"${VP_MODEL_SOURCES}\", incs=\"${VP_MODEL_INCLUDES}\"")

    # TODO verify arguments
    set(VP_MODEL_NAME_OPTIM "${VP_MODEL_NAME}_optim")
    set(VP_MODEL_NAME_DEBUG "${VP_MODEL_NAME}_debug")
    set(VP_MODEL_NAME_OPTIM_M32 "${VP_MODEL_NAME}_optim_m32")
    set(VP_MODEL_NAME_DEBUG_M32 "${VP_MODEL_NAME}_debug_m32")

    # ==================
    # Optimized models
    # ==================
    if(${BUILD_OPTIMIZED})
        add_library(${VP_MODEL_NAME_OPTIM} STATIC ${VP_MODEL_SOURCES})
        target_link_libraries(${VP_MODEL_NAME_OPTIM} PRIVATE gvsoc)
        set_target_properties(${VP_MODEL_NAME_OPTIM} PROPERTIES PREFIX "")
        target_compile_options(${VP_MODEL_NAME_OPTIM} PRIVATE -fno-stack-protector -D__GVSOC__)

        target_include_directories(${VP_MODEL_NAME_OPTIM} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

        foreach(X IN LISTS GVSOC_MODULES)
            target_include_directories(${VP_MODEL_NAME_OPTIM} PRIVATE ${X})
        endforeach()

        foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
            target_include_directories(${VP_MODEL_NAME_OPTIM} PRIVATE ${subdir})
        endforeach()

        if(VP_MODEL_OUTPUT_NAME)
            set(RENAME_OPTIM_NAME ${VP_MODEL_OUTPUT_NAME})
        else()
            set(RENAME_OPTIM_NAME ${VP_MODEL_NAME_OPTIM})
        endif()

        install(
            FILES $<TARGET_FILE:${VP_MODEL_NAME_OPTIM}>
            DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_OPTIM_INSTALL_FOLDER}/${VP_MODEL_PREFIX}"
            RENAME "${RENAME_OPTIM_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
    endif()

    if(${BUILD_OPTIMIZED_M32})
        add_library(${VP_MODEL_NAME_OPTIM_M32} STATIC ${VP_MODEL_SOURCES})
        target_link_libraries(${VP_MODEL_NAME_OPTIM_M32} PRIVATE gvsoc_m32)
        set_target_properties(${VP_MODEL_NAME_OPTIM_M32} PROPERTIES PREFIX "")
        target_compile_options(${VP_MODEL_NAME_OPTIM_M32} PRIVATE "-D__GVSOC__")
        target_compile_options(${VP_MODEL_NAME_OPTIM_M32} PRIVATE -m32 -D__M32_MODE__=1)
        target_link_options(${VP_MODEL_NAME_OPTIM_M32} PRIVATE -m32)
        target_include_directories(${VP_MODEL_NAME_OPTIM_M32} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

        foreach(X IN LISTS GVSOC_MODULES)
            target_include_directories(${VP_MODEL_NAME_OPTIM_M32} PRIVATE ${X})
        endforeach()

        foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
            target_include_directories(${VP_MODEL_NAME_OPTIM_M32} PRIVATE ${subdir})
        endforeach()

        if(VP_MODEL_OUTPUT_NAME)
            set(RENAME_OPTIM_M32_NAME ${VP_MODEL_OUTPUT_NAME})
        else()
            set(RENAME_OPTIM_M32_NAME ${VP_MODEL_NAME_OPTIM_M32})
        endif()

        install(
            FILES $<TARGET_FILE:${VP_MODEL_NAME_OPTIM_M32}>
            DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_OPTIM_M32_INSTALL_FOLDER}/${VP_MODEL_PREFIX}"
            RENAME "${RENAME_OPTIM_M32_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
    endif()

    # ==================
    # Debug models
    # ==================
    if(${BUILD_DEBUG})
        add_library(${VP_MODEL_NAME_DEBUG} STATIC ${VP_MODEL_SOURCES})
        target_link_libraries(${VP_MODEL_NAME_DEBUG} PRIVATE gvsoc_debug)
        set_target_properties(${VP_MODEL_NAME_DEBUG} PROPERTIES PREFIX "")
        target_compile_options(${VP_MODEL_NAME_DEBUG} PRIVATE "-D__GVSOC__")
        target_compile_definitions(${VP_MODEL_NAME_DEBUG} PRIVATE -DVP_TRACE_ACTIVE=1 -DVP_MEMCHECK_ACTIVE=1)

        target_include_directories(${VP_MODEL_NAME_DEBUG} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

        foreach(X IN LISTS GVSOC_MODULES)
            target_include_directories(${VP_MODEL_NAME_DEBUG} PRIVATE ${X})
        endforeach()

        foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
            target_include_directories(${VP_MODEL_NAME_DEBUG} PRIVATE ${subdir})
        endforeach()

        if(VP_MODEL_OUTPUT_NAME)
            set(RENAME_DEBUG_NAME ${VP_MODEL_OUTPUT_NAME})
        else()
            set(RENAME_DEBUG_NAME ${VP_MODEL_NAME_DEBUG})
        endif()

        install(
            FILES $<TARGET_FILE:${VP_MODEL_NAME_DEBUG}>
            DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_DEBUG_INSTALL_FOLDER}/${VP_MODEL_PREFIX}"
            RENAME "${RENAME_DEBUG_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
    endif()

    if(${BUILD_DEBUG_M32})
        add_library(${VP_MODEL_NAME_DEBUG_M32} STATIC ${VP_MODEL_SOURCES})
        target_link_libraries(${VP_MODEL_NAME_DEBUG_M32} PRIVATE gvsoc_debug_m32)
        set_target_properties(${VP_MODEL_NAME_DEBUG_M32} PROPERTIES PREFIX "")
        target_compile_options(${VP_MODEL_NAME_DEBUG_M32} PRIVATE "-D__GVSOC__")
        target_compile_options(${VP_MODEL_NAME_DEBUG_M32} PRIVATE -m32 -D__M32_MODE__=1)
        target_link_options(${VP_MODEL_NAME_DEBUG_M32} PRIVATE -m32)
        target_compile_definitions(${VP_MODEL_NAME_DEBUG_M32} PRIVATE -DVP_TRACE_ACTIVE=1 -DVP_MEMCHECK_ACTIVE=1)

        target_include_directories(${VP_MODEL_NAME_DEBUG_M32} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

        foreach(X IN LISTS GVSOC_MODULES)
            target_include_directories(${VP_MODEL_NAME_DEBUG_M32} PRIVATE ${X})
        endforeach()

        foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
            target_include_directories(${VP_MODEL_NAME_DEBUG_M32} PRIVATE ${subdir})
        endforeach()

        if(VP_MODEL_OUTPUT_NAME)
            set(RENAME_DEBUG_M32_NAME ${VP_MODEL_OUTPUT_NAME})
        else()
            set(RENAME_DEBUG_M32_NAME ${VP_MODEL_NAME_DEBUG_M32})
        endif()

        install(
            FILES $<TARGET_FILE:${VP_MODEL_NAME_DEBUG_M32}>
            DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_DEBUG_M32_INSTALL_FOLDER}/${VP_MODEL_PREFIX}"
            RENAME "${RENAME_DEBUG_M32_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
    endif()

endfunction()

function(vp_block_compile_options)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "OPTIONS"
        ${ARGN}
        )

    foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
        set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
        target_compile_options(${VP_MODEL_NAME_TYPE} PUBLIC ${VP_MODEL_OPTIONS})
    endforeach()
endfunction()

# vp_model function
function(vp_model)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;PREFIX;OUTPUT_NAME;FORCE_BUILD;"
        "SOURCES;INCLUDES"
        ${ARGN}
        )
    #message(STATUS "vp_model: name=\"${VP_MODEL_NAME}\", output_name=\"${VP_MODEL_OUTPUT_NAME}\" prefix=\"${VP_MODEL_DIRECTORY}\", srcs=\"${VP_MODEL_SOURCES}\", incs=\"${VP_MODEL_INCLUDES}\"")

    string(REPLACE "." "/" VP_MODEL_PATH ${VP_MODEL_NAME})

    get_filename_component(VP_MODEL_FILENAME ${VP_MODEL_PATH} NAME)
    get_filename_component(VP_MODEL_DIRECTORY ${VP_MODEL_PATH} DIRECTORY)

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)

        # TODO verify arguments
        set(VP_MODEL_NAME_OPTIM "${VP_MODEL_NAME}_optim")
        set(VP_MODEL_NAME_DEBUG "${VP_MODEL_NAME}_debug")
        set(VP_MODEL_NAME_OPTIM_M32 "${VP_MODEL_NAME}_optim_m32")
        set(VP_MODEL_NAME_DEBUG_M32 "${VP_MODEL_NAME}_debug_m32")

        # ==================
        # Optimized models
        # ==================
        if(${BUILD_OPTIMIZED})
            add_library(${VP_MODEL_NAME_OPTIM} MODULE ${VP_MODEL_SOURCES})
            target_link_libraries(${VP_MODEL_NAME_OPTIM} PRIVATE gvsoc)
            set_target_properties(${VP_MODEL_NAME_OPTIM} PROPERTIES PREFIX "")
            target_compile_options(${VP_MODEL_NAME_OPTIM} PRIVATE -fno-stack-protector -D__GVSOC__)
            target_include_directories(${VP_MODEL_NAME_OPTIM} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            foreach(X IN LISTS GVSOC_MODULES)
                target_include_directories(${VP_MODEL_NAME_OPTIM} PRIVATE ${X})
            endforeach()

            foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
                target_include_directories(${VP_MODEL_NAME_OPTIM} PRIVATE ${subdir})
            endforeach()

            if(DEFINED ENV{SYSTEMC_HOME})
                target_include_directories(${VP_MODEL_NAME_OPTIM} PRIVATE $ENV{SYSTEMC_HOME}/include)
            endif()

            if(VP_MODEL_OUTPUT_NAME)
                set(RENAME_OPTIM_NAME ${VP_MODEL_OUTPUT_NAME})
            else()
                set(RENAME_OPTIM_NAME ${VP_MODEL_FILENAME})
            endif()

            install(
                FILES $<TARGET_FILE:${VP_MODEL_NAME_OPTIM}>
                DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_OPTIM_INSTALL_FOLDER}/${VP_MODEL_DIRECTORY}"
                RENAME "${RENAME_OPTIM_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
                )
        endif()

        if(${BUILD_OPTIMIZED_M32})
            add_library(${VP_MODEL_NAME_OPTIM_M32} MODULE ${VP_MODEL_SOURCES})
            target_link_libraries(${VP_MODEL_NAME_OPTIM_M32} PRIVATE gvsoc_m32)
            set_target_properties(${VP_MODEL_NAME_OPTIM_M32} PROPERTIES PREFIX "")
            target_compile_options(${VP_MODEL_NAME_OPTIM_M32} PRIVATE "-D__GVSOC__")
            target_include_directories(${VP_MODEL_NAME_OPTIM_M32} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            target_compile_options(${VP_MODEL_NAME_OPTIM_M32} PRIVATE -m32 -D__M32_MODE__=1)
            target_link_options(${VP_MODEL_NAME_OPTIM_M32} PRIVATE -m32)
            foreach(X IN LISTS GVSOC_MODULES)
                target_include_directories(${VP_MODEL_NAME_OPTIM_M32} PRIVATE ${X})
            endforeach()

            foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
                target_include_directories(${VP_MODEL_NAME_OPTIM_M32} PRIVATE ${subdir})
            endforeach()

            if(VP_MODEL_OUTPUT_NAME)
                set(RENAME_OPTIM_M32_NAME ${VP_MODEL_OUTPUT_NAME})
            else()
                set(RENAME_OPTIM_M32_NAME ${VP_MODEL_FILENAME})
            endif()

            install(
                FILES $<TARGET_FILE:${VP_MODEL_NAME_OPTIM_M32}>
                DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_OPTIM_M32_INSTALL_FOLDER}/${VP_MODEL_DIRECTORY}"
                RENAME "${RENAME_OPTIM_M32_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
                )
        endif()

        # ==================
        # Debug models
        # ==================
        if(${BUILD_DEBUG})
            add_library(${VP_MODEL_NAME_DEBUG} MODULE ${VP_MODEL_SOURCES})
            target_link_libraries(${VP_MODEL_NAME_DEBUG} PRIVATE gvsoc_debug)
            set_target_properties(${VP_MODEL_NAME_DEBUG} PROPERTIES PREFIX "")
            target_compile_options(${VP_MODEL_NAME_DEBUG} PRIVATE "-D__GVSOC__")
            target_compile_definitions(${VP_MODEL_NAME_DEBUG} PRIVATE -DVP_TRACE_ACTIVE=1 -DVP_MEMCHECK_ACTIVE=1)
            target_include_directories(${VP_MODEL_NAME_DEBUG} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            foreach(X IN LISTS GVSOC_MODULES)
                target_include_directories(${VP_MODEL_NAME_DEBUG} PRIVATE ${X})
            endforeach()

            foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
                target_include_directories(${VP_MODEL_NAME_DEBUG} PRIVATE ${subdir})
            endforeach()

            if(DEFINED ENV{SYSTEMC_HOME})
                target_include_directories(${VP_MODEL_NAME_DEBUG} PRIVATE $ENV{SYSTEMC_HOME}/include)
            endif()

            if(VP_MODEL_OUTPUT_NAME)
                set(RENAME_DEBUG_NAME ${VP_MODEL_OUTPUT_NAME})
            else()
                set(RENAME_DEBUG_NAME ${VP_MODEL_FILENAME})
            endif()

            install(
                FILES $<TARGET_FILE:${VP_MODEL_NAME_DEBUG}>
                DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_DEBUG_INSTALL_FOLDER}/${VP_MODEL_DIRECTORY}"
                RENAME "${RENAME_DEBUG_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
                )
        endif()

        if(${BUILD_DEBUG_M32})
            add_library(${VP_MODEL_NAME_DEBUG_M32} MODULE ${VP_MODEL_SOURCES})
            target_link_libraries(${VP_MODEL_NAME_DEBUG_M32} PRIVATE gvsoc_debug_m32)
            set_target_properties(${VP_MODEL_NAME_DEBUG_M32} PROPERTIES PREFIX "")
            target_compile_options(${VP_MODEL_NAME_DEBUG_M32} PRIVATE "-D__GVSOC__")
            target_compile_options(${VP_MODEL_NAME_DEBUG_M32} PRIVATE -m32 -D__M32_MODE__=1)
            target_link_options(${VP_MODEL_NAME_DEBUG_M32} PRIVATE -m32)
            target_compile_definitions(${VP_MODEL_NAME_DEBUG_M32} PRIVATE -DVP_TRACE_ACTIVE=1 -DVP_MEMCHECK_ACTIVE=1)
            target_include_directories(${VP_MODEL_NAME_DEBUG_M32} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            foreach(X IN LISTS GVSOC_MODULES)
                target_include_directories(${VP_MODEL_NAME_DEBUG_M32} PRIVATE ${X})
            endforeach()

            foreach(subdir ${VP_MODEL_INCLUDE_DIRS})
                target_include_directories(${VP_MODEL_NAME_DEBUG_M32} PRIVATE ${subdir})
            endforeach()

            if(VP_MODEL_OUTPUT_NAME)
                set(RENAME_DEBUG_M32_NAME ${VP_MODEL_OUTPUT_NAME})
            else()
                set(RENAME_DEBUG_M32_NAME ${VP_MODEL_FILENAME})
            endif()

            install(
                FILES $<TARGET_FILE:${VP_MODEL_NAME_DEBUG_M32}>
                DESTINATION  "${GVSOC_MODELS_INSTALL_FOLDER}/${GVSOC_MODELS_DEBUG_M32_INSTALL_FOLDER}/${VP_MODEL_DIRECTORY}"
                RENAME "${RENAME_DEBUG_M32_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
                )
        endif()

        if (NOT "${CONFIG_CFLAGS_${VP_MODEL_NAME}}" STREQUAL "")
            foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
                set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
                string(REPLACE " " ";" CFLAGS_LIST "${CONFIG_CFLAGS_${VP_MODEL_NAME}}")
                foreach (CFLAG IN LISTS CFLAGS_LIST)
                    target_compile_options(${VP_MODEL_NAME_TYPE} PRIVATE ${CFLAG})
                endforeach()
            endforeach()
        endif()

    endif()
endfunction()

function(vp_model_link_gvsoc)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "LIBRARY"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TARGET "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_link_libraries(${VP_MODEL_NAME_TARGET} PRIVATE gvsoc${TARGET_TYPE})
        endforeach()
    endif()
endfunction()

function(vp_block_link_libraries)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;NO_M32;"
        "LIBRARY"
        ${ARGN}
    )

    if(TARGET_TYPES)
    else()
        set(TARGET_TYPES ${VP_TARGET_TYPES})
    endif()
    foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
        if (VP_MODEL_NO_M32 AND (TARGET_TYPE STREQUAL _debug_m32 OR TARGET_TYPE STREQUAL _optim_m32))
        else()
            set(VP_MODEL_NAME_TARGET "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_link_libraries(${VP_MODEL_NAME_TARGET} PRIVATE ${VP_MODEL_LIBRARY})
        endif()
    endforeach()
endfunction()

function(vp_model_link_libraries)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;NO_M32;FORCE_BUILD;"
        "LIBRARY"
        ${ARGN}
        )

        if(TARGET_TYPES)
        else()
            set(TARGET_TYPES ${VP_TARGET_TYPES})
        endif()

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            if (VP_MODEL_NO_M32 AND (TARGET_TYPE STREQUAL _debug_m32 OR TARGET_TYPE STREQUAL _optim_m32))
            else()
                set(VP_MODEL_NAME_TARGET "${VP_MODEL_NAME}${TARGET_TYPE}")
                target_link_libraries(${VP_MODEL_NAME_TARGET} PRIVATE ${VP_MODEL_LIBRARY})
            endif()
        endforeach()
    endif()
endfunction()

function(vp_model_link_blocks)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "BLOCK"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TARGET "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_link_libraries(${VP_MODEL_NAME_TARGET} PRIVATE ${VP_MODEL_BLOCK}${TARGET_TYPE})
        endforeach()
    endif()
endfunction()

function(vp_model_compile_options)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "OPTIONS"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_compile_options(${VP_MODEL_NAME_TYPE} PRIVATE ${VP_MODEL_OPTIONS})
        endforeach()
    endif()
endfunction()

function(vp_model_link_options)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "OPTIONS"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_link_options(${VP_MODEL_NAME_TYPE} PRIVATE ${VP_MODEL_OPTIONS})
        endforeach()
    endif()
endfunction()

function(vp_model_compile_definitions)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;"
        "DEFINITIONS"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_compile_definitions(${VP_MODEL_NAME_TYPE} PRIVATE ${VP_MODEL_DEFINITIONS})
        endforeach()
    endif()
endfunction()

function(vp_model_include_directories)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "DIRECTORY"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_include_directories(${VP_MODEL_NAME_TYPE} PRIVATE ${VP_MODEL_DIRECTORY})
        endforeach()
    endif()
endfunction()

function(vp_model_sources)
    cmake_parse_arguments(
        VP_MODEL
        ""
        "NAME;FORCE_BUILD;"
        "SOURCES"
        ${ARGN}
        )

    if ("${CONFIG_${VP_MODEL_NAME}}" EQUAL "1" OR DEFINED CONFIG_BUILD_ALL OR DEFINED VP_MODEL_FORCE_BUILD)
        foreach (TARGET_TYPE IN LISTS VP_TARGET_TYPES)
            set(VP_MODEL_NAME_TYPE "${VP_MODEL_NAME}${TARGET_TYPE}")
            target_sources(${VP_MODEL_NAME_TYPE} PRIVATE ${VP_MODEL_SOURCES})
        endforeach()
    endif()
endfunction()

function(vp_files)
    cmake_parse_arguments(
        VP_FILES
        ""
        "PREFIX"
        "FILES"
        ${ARGN}
        )
    #message(STATUS "vp_files: prefix=\"${VP_FILES_PREFIX}\", files=\"${VP_FILES_FILES}\"")
    install(FILES ${VP_FILES_FILES}
        DESTINATION "generators/${VP_FILES_PREFIX}")
endfunction()

function(vp_precompiled_models)
    cmake_parse_arguments(
        VP_FILES
        ""
        "PREFIX"
        "FILES"
        ${ARGN}
        )
    #message(STATUS "vp_files: prefix=\"${VP_FILES_PREFIX}\", files=\"${VP_FILES_FILES}\"")
    install(FILES ${VP_FILES_FILES}
        DESTINATION "models/${VP_FILES_PREFIX}")
endfunction()
