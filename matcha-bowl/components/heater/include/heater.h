#pragma once

#include "heater_if.h"

/**
 * Matcha-bowl heater backend.
 *
 * Dual-channel AC heating with zero-crossing synchronization:
 *   - Relay (GPIO26): bulk on/off, engaged at power > 0%
 *   - TRIAC (GPIO19): phase-angle control for proportional power
 * Zero-crossing detected on GPIO36 (PC817 optocoupler).
 *
 * The set_power() interface maps to the combined relay+TRIAC:
 *   0%      → relay OFF, TRIAC OFF
 *   1–50%   → relay OFF, TRIAC at mapped phase angle
 *   51–100% → relay ON,  TRIAC at mapped phase angle
 */

/** Create a heater_t instance with the matcha-bowl AC backend. */
heater_t *heater_matcha_create(void);
