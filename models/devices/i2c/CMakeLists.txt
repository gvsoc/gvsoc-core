vp_model(NAME devices.i2c.i2c_bus
    SOURCES "i2c_bus.cpp"
    )

add_subdirectory(helper)
add_subdirectory(corruptor)
add_subdirectory(eeprom)

