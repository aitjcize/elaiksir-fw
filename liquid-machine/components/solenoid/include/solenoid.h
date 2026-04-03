#pragma once

#include "esp_err.h"
#include "tca9548.h"
#include "pca9685.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Solenoid valve driver.
 *
 * Solenoids are driven by DRV8841 H-bridge (via PCA9685 PWM).
 * Control is simple on/off: energize one coil direction to open,
 * de-energize to close (spring return).
 *
 * From schematic (DRV8841_C page):
 *   Solenoid valves use AOUT1/AOUT2 pairs from DRV8841.
 *   PCA9685 PWM channels drive AIN1/AIN2 on the DRV8841.
 */

typedef struct {
    tca9548_t  *mux;
    uint8_t     mux_channel;
    pca9685_t  *pwm;
    uint8_t     pwm_ch_open;    /* PCA9685 channel to energize (open) */
    uint8_t     pwm_ch_close;   /* PCA9685 channel for opposite direction */
} solenoid_cfg_t;

typedef struct {
    solenoid_cfg_t cfg;
    bool           is_open;
} solenoid_t;

esp_err_t solenoid_init(solenoid_t *sol, const solenoid_cfg_t *cfg);
esp_err_t solenoid_open(solenoid_t *sol);
esp_err_t solenoid_close(solenoid_t *sol);
bool      solenoid_is_open(solenoid_t *sol);
