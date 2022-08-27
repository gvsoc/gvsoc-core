vp_model(NAME devices.i2c.corruptor.i2c_corruptor
    SOURCES "i2c_corruptor.cpp"
    )

vp_model_link_libraries(NAME devices.i2c.corruptor.i2c_corruptor LIBRARY i2c_helper)
