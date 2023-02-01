add_subdirectory(mem_plug)

vp_model(NAME interco.interleaver_impl
    SOURCES "interleaver_impl.cpp"
    )

vp_model(NAME interco.testandset
    SOURCES "testandset.cpp"
    )

vp_model(NAME interco.converter_impl
    SOURCES "converter_impl.cpp"
    )

vp_model(NAME interco.bus_watchpoint_64
    SOURCES "bus_watchpoint.cpp"
    )
vp_model_compile_options(NAME interco.bus_watchpoint_64 OPTIONS "-DWORD_SIZE=64")

vp_model(NAME interco.bus_watchpoint_32
    SOURCES "bus_watchpoint.cpp"
    )
vp_model_compile_options(NAME interco.bus_watchpoint_32 OPTIONS "-DWORD_SIZE=32")

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
