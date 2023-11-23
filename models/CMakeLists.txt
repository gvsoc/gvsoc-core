cmake_minimum_required(VERSION 3.10)
project(gvsoc-models)

install(DIRECTORY . DESTINATION ${CMAKE_INSTALL_PREFIX}/generators
FILES_MATCHING PATTERN "*.py" PATTERN "*.json" PATTERN __pycache__ EXCLUDE)

add_subdirectory(board)
add_subdirectory(cache)
add_subdirectory(cpu)
add_subdirectory(devices)
add_subdirectory(gdbserver)
add_subdirectory(interco)
add_subdirectory(memory)
add_subdirectory(utils)
add_subdirectory(vp)


install(
    FILES gvsoc/gvsoc_control.py
    DESTINATION  python/gv
    )

install(
    FILES gvsoc/json_tools.py
    DESTINATION  python/gvsoc
    )

install(
    DIRECTORY regmap
    DESTINATION  python
    )
