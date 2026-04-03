#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

/**
 * TCA9555 — 16-bit I2C GPIO expander.
 *
 * Two 8-bit ports (P0, P1). Each pin configurable as input or output.
 * Active-low interrupt output when input state changes.
 */

typedef struct {
    i2c_master_dev_handle_t dev;
} tca9555_t;

/**
 * Initialize TCA9555.
 * @param gpio   Output: driver instance
 * @param bus    I2C master bus handle
 * @param addr   7-bit address (0x20–0x27, set by A0-A2 pins)
 */
esp_err_t tca9555_init(tca9555_t *gpio, i2c_master_bus_handle_t bus, uint8_t addr);

/**
 * Configure pin direction. Bit=1 means input, bit=0 means output.
 * @param port0_dir  Port 0 direction mask (8 bits)
 * @param port1_dir  Port 1 direction mask (8 bits)
 */
esp_err_t tca9555_set_direction(tca9555_t *gpio, uint8_t port0_dir, uint8_t port1_dir);

/**
 * Write output values.
 */
esp_err_t tca9555_write(tca9555_t *gpio, uint8_t port0_val, uint8_t port1_val);

/**
 * Read input values.
 */
esp_err_t tca9555_read(tca9555_t *gpio, uint8_t *port0_val, uint8_t *port1_val);

/**
 * Read/write as 16-bit value (port1 << 8 | port0).
 */
esp_err_t tca9555_write16(tca9555_t *gpio, uint16_t val);
esp_err_t tca9555_read16(tca9555_t *gpio, uint16_t *val);
