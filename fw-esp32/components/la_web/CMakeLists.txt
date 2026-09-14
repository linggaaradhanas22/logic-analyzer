idf_component_register(
    SRCS
        "src/la_web_core.c"
        "src/la_web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES la_board la_link la_proto esp_http_server esp_timer
    PRIV_REQUIRES esp_ringbuf
    EMBED_FILES "web/index.html"
)
