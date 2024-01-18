add_subdirectory(mem_plug)

vp_model(NAME interco.router_impl
    SOURCES "router_impl.cpp"
    )

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

vp_model(NAME interco.sequencer
    SOURCES "sequencer.cpp"
    )
