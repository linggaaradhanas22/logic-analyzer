idf_component_register(
    SRCS
        "src/la_net_policy.c"
        "src/net_wifi.c"
        "src/net_mdns.c"
        "src/net_server.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES la_board la_link la_proto esp_wifi esp_netif esp_event lwip mdns esp_timer
)
