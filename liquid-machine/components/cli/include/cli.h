#pragma once

#include "esp_err.h"
#include "motor_if.h"
#include "temp_sensor_if.h"
#include "weight_sensor_if.h"
#include "solenoid.h"
#include "flow_sensor.h"

typedef struct {
    motor_t         **motors;
    int               num_motors;
    temp_sensor_t   **temp_sensors;
    int               num_temp_sensors;
    weight_sensor_t **scales;
    int               num_scales;
    solenoid_t       *valves;
    int               num_valves;
    flow_sensor_t    *flow;
} cli_config_t;

/**
 * Initialize and start the CLI console over UART.
 */
esp_err_t cli_init(const cli_config_t *config);
