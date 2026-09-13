idf_component_register(
    SRCS
        "src/la_relay.c"
        "src/la_link_uart.c"
        "src/la_link_tasks.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES la_board la_proto esp_driver_uart esp_timer
    PRIV_REQUIRES esp_ringbuf esp_driver_gpio esp_driver_spi heap
)
