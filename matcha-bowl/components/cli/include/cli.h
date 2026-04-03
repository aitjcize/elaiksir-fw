#pragma once

#include "esp_err.h"
#include "motor_if.h"
#include "temp_sensor_if.h"
#include "heater_if.h"

typedef struct {
    heater_t      *heater;
    motor_t       *motor;
    temp_sensor_t *temp_sensor;
} cli_config_t;

/**
 * Initialize and start the CLI console over UART.
 * Registers all test commands. Non-blocking — runs in its own task.
 */
esp_err_t cli_init(const cli_config_t *config);
