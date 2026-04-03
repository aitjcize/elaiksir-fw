#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * Flow sensor driver — pulse counting via PCNT peripheral.
 *
 * Optical flow sensor (LTV-217-G optocoupler) generates pulses
 * proportional to liquid flow. PCNT counts pulses; software
 * calculates flow rate and total volume.
 *
 * Calibration: pulses_per_ml must be determined experimentally.
 */

typedef struct {
    int64_t  total_pulses;
    float    pulses_per_ml;     /* Calibration factor */
    float    flow_rate_ml_s;    /* Current flow rate in mL/s */
} flow_sensor_t;

/**
 * Initialize flow sensor PCNT on the configured GPIO.
 * @param pulses_per_ml  Calibration: pulses per milliliter
 */
esp_err_t flow_sensor_init(flow_sensor_t *fs, float pulses_per_ml);

/** Reset total pulse count and volume. */
void flow_sensor_reset(flow_sensor_t *fs);

/** Get total volume dispensed since last reset, in mL. */
float flow_sensor_get_volume_ml(flow_sensor_t *fs);

/** Get current flow rate in mL/s. Call periodically (~100ms) for accuracy. */
float flow_sensor_get_rate_ml_s(flow_sensor_t *fs);

/** Update readings — call this periodically from a task. */
esp_err_t flow_sensor_update(flow_sensor_t *fs);
