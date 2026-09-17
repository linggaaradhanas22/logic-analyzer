set(la_usb_requires la_board la_link la_proto)

if(IDF_TARGET STREQUAL "esp32s3")
    list(APPEND la_usb_requires esp_tinyusb)
endif()

idf_component_register(
    SRCS "src/usb_server.c"
    INCLUDE_DIRS "include"
    REQUIRES ${la_usb_requires}
)
