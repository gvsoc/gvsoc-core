vp_model(NAME utils.loader_impl
    SOURCES "loader_impl.cpp"
    )

vp_model_link_libraries(NAME utils.injector_impl LIBRARY gvsoc_launcher_lib)

vp_model(NAME utils.composite_impl
    SOURCES "composite_impl.cpp"
    )

vp_model(NAME utils.clock_impl
    SOURCES "clock_impl.cpp"
    )

vp_model(NAME utils.dpi_chip_wrapper
    FORCE_BUILD 1
    SOURCES "dpi_chip_wrapper.cpp"
    )

add_subdirectory(loader)