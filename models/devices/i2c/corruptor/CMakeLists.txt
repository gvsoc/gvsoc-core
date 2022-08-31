vp_model(NAME devices.i2c.corruptor.i2c_corruptor
    SOURCES "i2c_corruptor.cpp"
    )

vp_model_link_blocks(
    NAME devices.i2c.corruptor.i2c_corruptor
    BLOCK i2c_helper
    )
