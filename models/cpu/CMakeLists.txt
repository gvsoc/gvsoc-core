add_subdirectory(iss)
add_subdirectory(emulation)


vp_model(NAME cpu.clint
    SOURCES "clint.cpp"
)

vp_model(NAME cpu.plic
    SOURCES "plic.cpp"
)
