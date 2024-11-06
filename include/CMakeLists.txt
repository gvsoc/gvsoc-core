message(DEBUG "============================ Creating gvsoc target library")
add_library(gvsoc_target INTERFACE)
target_include_directories(gvsoc_target INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
