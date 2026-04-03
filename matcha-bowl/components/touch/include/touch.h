#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Touch button driver — reads TTP223-TD capacitive touch ICs.
 * Each TTP223 outputs high when touched.  4 buttons: SW1–SW4.
 */

typedef enum {
    TOUCH_BTN_SW1 = 0,
    TOUCH_BTN_SW2,
    TOUCH_BTN_SW3,
    TOUCH_BTN_SW4,
    TOUCH_BTN_COUNT,
} touch_btn_t;

typedef void (*touch_event_cb_t)(touch_btn_t btn, bool pressed);

/**
 * Initialize touch button polling.
 */
esp_err_t touch_init(void);

/**
 * Register a callback for touch events (press/release).
 * Called from touch polling task context.
 */
void touch_register_callback(touch_event_cb_t cb);

/**
 * Read instantaneous state of a button. True = touched.
 */
bool touch_is_pressed(touch_btn_t btn);

/**
 * Read all buttons as a bitmask (bit 0 = SW1, etc).
 */
uint8_t touch_read_all(void);
