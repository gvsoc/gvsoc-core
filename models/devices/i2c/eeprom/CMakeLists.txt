vp_model(NAME devices.i2c.eeprom.i2c_eeprom
    SOURCES "i2c_eeprom.cpp"
    )

vp_model_link_libraries(NAME devices.i2c.eeprom.i2c_eeprom LIBRARY i2c_helper)
