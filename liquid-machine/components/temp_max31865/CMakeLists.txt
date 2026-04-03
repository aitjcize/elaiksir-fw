idf_component_register(
    SRCS "temp_max31865.c"
    INCLUDE_DIRS "include"
    REQUIRES driver board_hal temp_sensor_if drv_max31865
)
