#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Abstract heater interface.
 *
 * Supports both on/off and proportional (0–100%) control.
 * Backend implementations handle the specifics:
 *   - matcha-bowl: AC relay (on/off) + TRIAC (phase-angle %)
 *   - future devices: DC SSR, PWM heating element, etc.
 */

typedef struct heater heater_t;

typedef struct heater_ops {
    esp_err_t (*init)(heater_t *heater);
    void      (*set_power)(heater_t *heater, int percent);  /* 0–100% */
    void      (*shutdown)(heater_t *heater);                 /* Emergency off */
    bool      (*is_active)(heater_t *heater);
} heater_ops_t;

struct heater {
    const char          *name;
    const heater_ops_t  *ops;
    void                *ctx;
};

esp_err_t heater_init(heater_t *heater);
void      heater_set_power(heater_t *heater, int percent);
void      heater_shutdown(heater_t *heater);
bool      heater_is_active(heater_t *heater);
