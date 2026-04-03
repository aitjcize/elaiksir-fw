idf_component_register(
    SRCS "temp_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES board_hal esp_adc temp_sensor_if
)
