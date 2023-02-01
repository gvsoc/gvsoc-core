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

vp_model(NAME interco.router_shared_fullduplex_impl
    SOURCES "router_shared_fullduplex_impl.cpp"
    )

vp_model(NAME interco.router_shared_halfduplex_impl
    SOURCES "router_shared_halfduplex_impl.cpp"
    )

vp_model(NAME interco.router_shared_async_impl
    SOURCES "router_shared_async_impl.cpp"
    )

vp_model(NAME interco.wireless_link_impl
    SOURCES "wireless_link_impl.cpp"
    )

vp_model(NAME interco.dual_wireless_link_impl
    SOURCES "dual_wireless_link_impl.cpp"
    )
