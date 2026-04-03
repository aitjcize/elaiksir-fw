#pragma once

#include "motor_if.h"

/**
 * Matcha-bowl motor backend.
 *
 * Direct LEDC PWM → MOSFET (40N05) on GPIO23.
 * Current sense: 5mΩ shunt (R34) → ADC on GPIO34.
 * Forward-only (no H-bridge), clamps negative speed to 0.
 */

/** Create a motor_t instance with the matcha-bowl LEDC backend. */
motor_t *motor_matcha_create(void);
