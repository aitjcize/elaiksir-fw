idf_component_register(
    SRCS "solenoid.c"
    INCLUDE_DIRS "include"
    REQUIRES driver drv_tca9548 drv_pca9685
)
