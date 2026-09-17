idf_component_register(
    SRCS "main.c" "ui_task.c"
    INCLUDE_DIRS "."
    REQUIRES la_board la_link la_net la_usb la_web nvs_flash esp_driver_gpio
)
