#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Abstract temperature sensor interface.
 *
 * Backend implementations:
 *   - matcha-bowl: NTC thermistor via ADC + B-parameter equation
 *   - liquid-machine: PT100/PT1000 via MAX31865 SPI
 */

typedef struct temp_sensor temp_sensor_t;

typedef struct temp_sensor_ops {
    esp_err_t (*init)(temp_sensor_t *sensor);
    int       (*read_deci_c)(temp_sensor_t *sensor);    /* Returns temp × 10, e.g. 850 = 85.0°C */
    int       (*read_resistance)(temp_sensor_t *sensor); /* Raw sensor resistance in ohms */
    bool      (*is_healthy)(temp_sensor_t *sensor);
} temp_sensor_ops_t;

struct temp_sensor {
    const char              *name;
    const temp_sensor_ops_t *ops;
    void                    *ctx;
};

esp_err_t temp_sensor_init(temp_sensor_t *sensor);
int       temp_sensor_read_deci_c(temp_sensor_t *sensor);
int       temp_sensor_read_resistance(temp_sensor_t *sensor);
bool      temp_sensor_is_healthy(temp_sensor_t *sensor);
