idf_component_register(
    SRCS "ethernet.c"
    INCLUDE_DIRS "include"
    REQUIRES driver board_hal esp_eth esp_netif esp_event
)
