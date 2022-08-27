add_subdirectory(mem_plug)

vp_model(NAME interco.router_impl
    SOURCES "router_impl.cpp"
    )

vp_model(NAME interco.interleaver_impl
    SOURCES "interleaver_impl.cpp"
    )

vp_model(NAME interco.converter_impl
    SOURCES "converter_impl.cpp"
    )

vp_model(NAME interco.bus_watchpoint
    SOURCES "bus_watchpoint.cpp"
    )
