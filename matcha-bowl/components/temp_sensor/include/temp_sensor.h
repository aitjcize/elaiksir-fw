#pragma once

#include "temp_sensor_if.h"

/**
 * Matcha-bowl NTC temperature sensor backend.
 *
 * 10K NTC thermistor in voltage divider: 3.3V — R25(10K) — ADC(GPIO39) — NTC — GND.
 * Uses B-parameter equation for temperature conversion.
 */

/** Create a temp_sensor_t instance with the matcha-bowl NTC backend. */
temp_sensor_t *temp_sensor_ntc_create(void);
