idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES unity la_board la_link la_proto esp_driver_uart esp_driver_gpio
    PRIV_REQUIRES esp_ringbuf
)
