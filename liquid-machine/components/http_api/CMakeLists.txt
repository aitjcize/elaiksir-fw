idf_component_register(
    SRCS "http_api.c" "api_pump.c" "api_order.c" "api_settings.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json motor_if weight_sensor_if temp_sensor_if nvs_flash
)
