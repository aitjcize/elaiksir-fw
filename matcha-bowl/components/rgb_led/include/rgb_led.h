#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * RGB LED driver.
 *
 * Power switch (Q1 MOSFET) on GPIO27 — enables/disables 12V to LED strip.
 * PWM brightness on GPIO14 via LEDC.
 */

esp_err_t rgb_led_init(void);

/** Enable or disable RGB LED power. */
void rgb_led_enable(bool on);

/** Set brightness 0–100%. */
void rgb_led_set_brightness(int percent);
