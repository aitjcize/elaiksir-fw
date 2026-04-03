idf_component_register(
    SRCS "load_cell.c"
    INCLUDE_DIRS "include"
    REQUIRES driver board_hal weight_sensor_if drv_tca9548 drv_nau7802
)
