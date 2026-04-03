#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Abstract motor interface.
 *
 * Each product provides a backend implementation:
 *   - matcha-bowl: direct LEDC PWM → MOSFET
 *   - liquid-machine: I2C → PCA9685 → DRV8841 H-bridge
 *
 * Speed range: -100 to +100 (negative = reverse, for H-bridge backends).
 * Backends that only support forward (e.g. single MOSFET) clamp to 0–100.
 */

typedef struct motor motor_t;

typedef struct motor_ops {
    esp_err_t (*init)(motor_t *motor);
    void      (*set_speed)(motor_t *motor, int percent);
    void      (*stop)(motor_t *motor);
    int       (*read_current_ma)(motor_t *motor);
    bool      (*is_running)(motor_t *motor);
} motor_ops_t;

struct motor {
    const char      *name;
    const motor_ops_t *ops;
    void            *ctx;       /* Backend-specific context */
};

esp_err_t motor_init(motor_t *motor);
void      motor_set_speed(motor_t *motor, int percent);
void      motor_stop(motor_t *motor);
int       motor_read_current_ma(motor_t *motor);
bool      motor_is_running(motor_t *motor);
