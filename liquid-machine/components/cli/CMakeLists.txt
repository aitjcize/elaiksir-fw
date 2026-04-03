idf_component_register(
    SRCS "cli.c"
    INCLUDE_DIRS "include"
    REQUIRES console board_hal motor_if temp_sensor_if weight_sensor_if
             solenoid flow_sensor ethernet
             drv_tca9548 drv_tca9555
)
