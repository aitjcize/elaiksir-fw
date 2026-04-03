idf_component_register(
    SRCS "motor_drv8841.c"
    INCLUDE_DIRS "include"
    REQUIRES driver board_hal motor_if drv_tca9548 drv_pca9685 drv_ads1115
)
