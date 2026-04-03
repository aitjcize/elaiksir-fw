idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES board_hal heater motor temp_sensor touch rgb_led display
                  motor_if temp_sensor_if heater_if
)
