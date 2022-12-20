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

vp_model(NAME interco.router_proxy
    SOURCES "router_proxy.cpp"
    )

vp_model(NAME interco.router_shared_impl
    SOURCES "router_shared_impl.cpp"
    )

vp_model(NAME interco.router_shared_async_impl
    SOURCES "router_shared_async_impl.cpp"
    )
