idf_component_register(
    SRCS "heater.c"
    INCLUDE_DIRS "include"
    REQUIRES driver board_hal heater_if
)
