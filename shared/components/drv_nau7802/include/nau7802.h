#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * NAU7802 — 24-bit dual-channel ADC for bridge sensors (load cells).
 *
 * Fixed I2C address 0x2A. Multiple instances require I2C mux (TCA9548).
 * Supports gains of 1×–128× and sample rates of 10–320 SPS.
 */

typedef enum {
    NAU7802_GAIN_1   = 0,
    NAU7802_GAIN_2   = 1,
    NAU7802_GAIN_4   = 2,
    NAU7802_GAIN_8   = 3,
    NAU7802_GAIN_16  = 4,
    NAU7802_GAIN_32  = 5,
    NAU7802_GAIN_64  = 6,
    NAU7802_GAIN_128 = 7,
} nau7802_gain_t;

typedef enum {
    NAU7802_SPS_10  = 0,
    NAU7802_SPS_20  = 1,
    NAU7802_SPS_40  = 2,
    NAU7802_SPS_80  = 3,
    NAU7802_SPS_320 = 7,
} nau7802_sps_t;

typedef enum {
    NAU7802_CH1 = 0,
    NAU7802_CH2 = 1,
} nau7802_channel_t;

typedef struct {
    i2c_master_dev_handle_t dev;
    nau7802_gain_t          gain;
} nau7802_t;

/**
 * Initialize NAU7802. Performs power-up, calibration, and configures gain/SPS.
 */
esp_err_t nau7802_init(nau7802_t *adc, i2c_master_bus_handle_t bus,
                       nau7802_gain_t gain, nau7802_sps_t sps);

/**
 * Select input channel (CH1 or CH2).
 */
esp_err_t nau7802_select_channel(nau7802_t *adc, nau7802_channel_t ch);

/**
 * Check if a new conversion result is ready.
 */
bool nau7802_data_ready(nau7802_t *adc);

/**
 * Read 24-bit conversion result (signed, two's complement).
 * Blocks until data is ready (up to timeout).
 */
esp_err_t nau7802_read_raw(nau7802_t *adc, int32_t *raw_out);

/**
 * Perform internal offset calibration.
 */
esp_err_t nau7802_calibrate(nau7802_t *adc);
