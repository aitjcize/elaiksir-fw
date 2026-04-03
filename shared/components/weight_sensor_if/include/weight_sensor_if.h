#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Abstract weight / load cell sensor interface.
 *
 * Backends:
 *   - liquid-machine: NAU7802 24-bit ADC via I2C mux
 */

typedef struct weight_sensor weight_sensor_t;

typedef struct weight_sensor_ops {
    esp_err_t (*init)(weight_sensor_t *sensor);
    int32_t   (*read_raw)(weight_sensor_t *sensor);
    int       (*read_grams)(weight_sensor_t *sensor);       /* Weight in grams */
    esp_err_t (*tare)(weight_sensor_t *sensor);              /* Zero / tare */
    esp_err_t (*calibrate)(weight_sensor_t *sensor, int known_grams); /* Calibrate with known weight */
    bool      (*is_stable)(weight_sensor_t *sensor);         /* Reading settled */
} weight_sensor_ops_t;

struct weight_sensor {
    const char                *name;
    const weight_sensor_ops_t *ops;
    void                      *ctx;
};

esp_err_t weight_sensor_init(weight_sensor_t *sensor);
int32_t   weight_sensor_read_raw(weight_sensor_t *sensor);
int       weight_sensor_read_grams(weight_sensor_t *sensor);
esp_err_t weight_sensor_tare(weight_sensor_t *sensor);
esp_err_t weight_sensor_calibrate(weight_sensor_t *sensor, int known_grams);
bool      weight_sensor_is_stable(weight_sensor_t *sensor);
