idf_component_register(
    SRCS "motor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver board_hal esp_adc motor_if
)
