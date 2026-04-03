#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

/**
 * ADS1115 — 16-bit, 4-channel ADC with PGA.
 *
 * Used for motor current sensing and analog input measurement.
 * Supports single-ended (AINx vs GND) and differential modes.
 */

typedef enum {
    ADS1115_MUX_AIN0_AIN1 = 0,     /* Differential A0-A1 */
    ADS1115_MUX_AIN0_AIN3 = 1,
    ADS1115_MUX_AIN1_AIN3 = 2,
    ADS1115_MUX_AIN2_AIN3 = 3,
    ADS1115_MUX_AIN0_GND  = 4,     /* Single-ended */
    ADS1115_MUX_AIN1_GND  = 5,
    ADS1115_MUX_AIN2_GND  = 6,
    ADS1115_MUX_AIN3_GND  = 7,
} ads1115_mux_t;

typedef enum {
    ADS1115_PGA_6144 = 0,   /* ±6.144V (LSB = 187.5µV) */
    ADS1115_PGA_4096 = 1,   /* ±4.096V (LSB = 125µV) */
    ADS1115_PGA_2048 = 2,   /* ±2.048V (default, LSB = 62.5µV) */
    ADS1115_PGA_1024 = 3,   /* ±1.024V (LSB = 31.25µV) */
    ADS1115_PGA_0512 = 4,   /* ±0.512V (LSB = 15.625µV) */
    ADS1115_PGA_0256 = 5,   /* ±0.256V (LSB = 7.8125µV) */
} ads1115_pga_t;

typedef enum {
    ADS1115_SPS_8   = 0,
    ADS1115_SPS_16  = 1,
    ADS1115_SPS_32  = 2,
    ADS1115_SPS_64  = 3,
    ADS1115_SPS_128 = 4,    /* Default */
    ADS1115_SPS_250 = 5,
    ADS1115_SPS_475 = 6,
    ADS1115_SPS_860 = 7,
} ads1115_sps_t;

typedef struct {
    i2c_master_dev_handle_t dev;
    ads1115_pga_t           pga;
    ads1115_sps_t           sps;
} ads1115_t;

/**
 * Initialize ADS1115.
 * @param adc    Output: driver instance
 * @param bus    I2C master bus handle
 * @param addr   7-bit address (0x48–0x4B depending on ADDR pin)
 * @param pga    Gain setting
 * @param sps    Sample rate
 */
esp_err_t ads1115_init(ads1115_t *adc, i2c_master_bus_handle_t bus,
                       uint8_t addr, ads1115_pga_t pga, ads1115_sps_t sps);

/**
 * Perform single-shot read on the given channel/mux config.
 * Blocks until conversion completes.
 * @param raw_out  Output: signed 16-bit result
 */
esp_err_t ads1115_read_raw(ads1115_t *adc, ads1115_mux_t mux, int16_t *raw_out);

/**
 * Convert raw ADC value to millivolts based on current PGA setting.
 */
int ads1115_raw_to_mv(ads1115_t *adc, int16_t raw);
