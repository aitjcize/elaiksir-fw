#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

/**
 * TCA9548A — 8-channel I2C multiplexer.
 *
 * Selects which downstream I2C channel(s) are connected to the upstream bus.
 * Write a single byte where each bit enables one channel (bit 0 = CH0, etc).
 * Typically only one channel is active at a time.
 */

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t                 current_channel;
} tca9548_t;

/**
 * Initialize TCA9548 on the given I2C bus.
 * @param mux      Output: driver instance
 * @param bus      I2C master bus handle
 * @param addr     7-bit I2C address (typically 0x70)
 */
esp_err_t tca9548_init(tca9548_t *mux, i2c_master_bus_handle_t bus, uint8_t addr);

/**
 * Select a single channel (0–7). Deselects all others.
 */
esp_err_t tca9548_select_channel(tca9548_t *mux, uint8_t channel);

/**
 * Disable all channels.
 */
esp_err_t tca9548_select_none(tca9548_t *mux);
