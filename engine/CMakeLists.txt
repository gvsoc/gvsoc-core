cmake_minimum_required(VERSION 3.10)
project(gvsoc-engine LANGUAGES CXX C)

set(GVSOC_ENGINE_CXX_SRCS
    "src/trace/lxt2.cpp"
    "src/trace/event.cpp"
    "src/trace/trace.cpp"
    "src/trace/raw/trace_dumper.cpp"
    "src/trace/raw.cpp"
    "src/trace/fst.cpp"
    "src/trace/vcd.cpp"
    "src/clock/clock.cpp"
    "src/clock/clock_engine.cpp"
    "src/clock/clock_event.cpp"
    "src/vp.cpp"
    "src/time_scheduler.cpp"
    "src/top.cpp"
    "src/block.cpp"
    "src/register.cpp"
    "src/signal.cpp"
    "src/queue.cpp"
    "src/proxy.cpp"
    "src/trace/trace_domain_impl.cpp"
    "src/power/power_table.cpp"
    "src/power/power_engine.cpp"
    "src/power/component_power.cpp"
    "src/power/power_trace.cpp"
    "src/power/power_source.cpp"
    "src/time_engine.cpp"
    "src/launcher.cpp"
    "src/proxy_client.cpp"
    "src/jsmn.cpp"
    "src/json.cpp"
    )

set(GVSOC_ENGINE_C_SRCS
    "src/trace/fst/lz4.c"
    "src/trace/fst/fastlz.c"
    "src/trace/fst/fstapi.c"
    "src/trace/lxt2_write.c"
    )

set(GVSOC_ENGINE_INC_DIRS "include")

# ==================
# Optimized launcher
# ==================

if(${BUILD_OPTIMIZED})
    add_library(gvsoc SHARED ${GVSOC_ENGINE_CXX_SRCS} ${GVSOC_ENGINE_C_SRCS})
    target_include_directories(gvsoc PUBLIC ${GVSOC_ENGINE_INC_DIRS})
    target_link_libraries(gvsoc PUBLIC z pthread ${CMAKE_DL_LIBS})
    set_target_properties(gvsoc PROPERTIES OUTPUT_NAME "pulpvp")
    install(TARGETS gvsoc
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        )

    add_executable(gvsoc_launcher "src/main.cpp")
    target_link_libraries(gvsoc_launcher PRIVATE gvsoc z pthread ${CMAKE_DL_LIBS})
    install(TARGETS gvsoc_launcher
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        )
endif()

if(${BUILD_OPTIMIZED_M32})
    add_library(gvsoc_m32 SHARED ${GVSOC_ENGINE_CXX_SRCS} ${GVSOC_ENGINE_C_SRCS})
    target_include_directories(gvsoc_m32 PUBLIC ${GVSOC_ENGINE_INC_DIRS})
    target_link_libraries(gvsoc_m32 PUBLIC z pthread ${CMAKE_DL_LIBS})
    set_target_properties(gvsoc_m32 PROPERTIES OUTPUT_NAME "pulpvp-m32")
    install(TARGETS gvsoc_m32   
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        INCLUDES DESTINATION include
        )
    target_compile_options(gvsoc_m32 PRIVATE -m32 -D__M32_MODE__)
    target_link_options(gvsoc_m32 PRIVATE -m32)
endif()

# ==============
# Debug launcher
# ==============

if(${BUILD_DEBUG})
    add_library(gvsoc_debug SHARED ${GVSOC_ENGINE_CXX_SRCS} ${GVSOC_ENGINE_C_SRCS})
    target_include_directories(gvsoc_debug PUBLIC ${GVSOC_ENGINE_INC_DIRS})
    target_link_libraries(gvsoc_debug PUBLIC z pthread ${CMAKE_DL_LIBS})
    set_target_properties(gvsoc_debug PROPERTIES OUTPUT_NAME "pulpvp-debug")
    install(TARGETS gvsoc_debug
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        )
    target_compile_definitions(gvsoc_debug PRIVATE "-DVP_TRACE_ACTIVE=1")

    add_executable(gvsoc_launcher_debug "src/main.cpp")
    target_link_libraries(gvsoc_launcher_debug PRIVATE gvsoc_debug z pthread ${CMAKE_DL_LIBS})
    install(TARGETS gvsoc_launcher_debug
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        )
    target_compile_definitions(gvsoc_launcher_debug PRIVATE "-DVP_TRACE_ACTIVE=1")
endif()

if(${BUILD_DEBUG_M32})
    add_library(gvsoc_debug_m32 SHARED ${GVSOC_ENGINE_CXX_SRCS} ${GVSOC_ENGINE_C_SRCS})
    target_include_directories(gvsoc_debug_m32 PUBLIC ${GVSOC_ENGINE_INC_DIRS})
    target_link_libraries(gvsoc_debug_m32 PUBLIC z pthread ${CMAKE_DL_LIBS})
    set_target_properties(gvsoc_debug_m32 PROPERTIES OUTPUT_NAME "pulpvp-debug-m32")
    install(TARGETS gvsoc_debug_m32
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        INCLUDES DESTINATION include
        )
    target_compile_definitions(gvsoc_debug_m32 PRIVATE "-DVP_TRACE_ACTIVE=1")
    target_compile_options(gvsoc_debug_m32 PRIVATE -m32 -D__M32_MODE__)
    target_link_options(gvsoc_debug_m32 PRIVATE "-m32")
endif()

# ==============
# Subdirectories
# ==============

add_subdirectory(vp)
