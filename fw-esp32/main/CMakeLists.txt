idf_component_register(
    SRCS "main.c" "ui_task.c"
    INCLUDE_DIRS "."
    REQUIRES la_board la_link la_net nvs_flash esp_driver_gpio
)
