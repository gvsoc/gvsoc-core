vp_model(NAME memory.memory
    SOURCES "memory.cpp"
    )

vp_model(NAME memory.memory_atomics
    SOURCES "memory.cpp"
    )
vp_model_compile_options(NAME memory.memory_atomics OPTIONS "-DCONFIG_ATOMICS=1")
