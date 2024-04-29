vp_model(NAME devices.sound.dac.ssm6515.ssm6515
    SOURCES "ssm6515.cpp"
    )

vp_model_link_blocks(
    NAME devices.sound.dac.ssm6515.ssm6515
    BLOCK pcm_pdm_conversion
    )

vp_model_link_blocks(
    NAME devices.sound.dac.ssm6515.ssm6515
    BLOCK io_audio
    )