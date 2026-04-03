idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES board_hal motor_if temp_sensor_if weight_sensor_if
                  motor_drv8841 temp_max31865 load_cell
                  flow_sensor solenoid ethernet http_api
                  drv_tca9548 drv_pca9685 drv_ads1115 drv_nau7802 drv_tca9555
)
