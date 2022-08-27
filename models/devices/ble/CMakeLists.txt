set(NINA_B112_SRCS "nina_b112.cpp"
    "nina_b112_cmd_parser.cpp")

vp_model(NAME devices.ble.nina_b112
    SOURCES ${NINA_B112_SRCS}
    )
