#pragma once

#include "weight_sensor_if.h"
#include "tca9548.h"
#include "nau7802.h"

/**
 * Liquid-machine load cell backend.
 *
 * NAU7802 24-bit ADC behind TCA9548 I2C mux.
 * Each load cell is on a separate mux channel (CH0–CH3).
 */

typedef struct {
    tca9548_t  *mux;
    uint8_t     mux_channel;
} load_cell_cfg_t;

/**
 * Create a weight_sensor_t with NAU7802 backend.
 * @param name         Sensor name (e.g. "scale1")
 * @param bus          I2C bus handle (the upstream bus the mux is on)
 * @param cfg          Mux and channel configuration
 */
weight_sensor_t *load_cell_create(const char *name,
                                   i2c_master_bus_handle_t bus,
                                   const load_cell_cfg_t *cfg);
