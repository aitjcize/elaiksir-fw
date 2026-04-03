idf_component_register(
    SRCS "cli.c"
    INCLUDE_DIRS "include"
    REQUIRES console board_hal heater motor temp_sensor touch rgb_led display
             heater_if motor_if temp_sensor_if
)
