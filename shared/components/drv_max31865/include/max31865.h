#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * MAX31865 — RTD-to-digital converter (PT100/PT1000).
 *
 * SPI interface. Converts platinum RTD resistance to a 15-bit digital value.
 * Supports 2/3/4-wire RTD configurations.
 */

typedef enum {
    MAX31865_WIRE_2 = 0,
    MAX31865_WIRE_3 = 1,    /* Most common for PT100 */
    MAX31865_WIRE_4 = 0,    /* Same config register value as 2-wire */
} max31865_wire_t;

typedef struct {
    spi_device_handle_t spi;
    float               r_ref;      /* Reference resistor value (e.g. 430Ω for PT100, 4300Ω for PT1000) */
    float               r_nominal;  /* RTD nominal resistance at 0°C (100Ω for PT100, 1000Ω for PT1000) */
} max31865_t;

/**
 * Initialize MAX31865 on the given SPI bus.
 * @param rtd        Output: driver instance
 * @param host       SPI host (e.g. SPI2_HOST)
 * @param cs_pin     Chip select GPIO
 * @param r_ref      Reference resistor in ohms (430 for PT100, 4300 for PT1000)
 * @param r_nominal  RTD nominal resistance at 0°C
 * @param wire_mode  2/3/4-wire configuration
 */
esp_err_t max31865_init(max31865_t *rtd, spi_host_device_t host, gpio_num_t cs_pin,
                        float r_ref, float r_nominal, max31865_wire_t wire_mode);

/**
 * Read raw 15-bit RTD ratio.
 */
esp_err_t max31865_read_raw(max31865_t *rtd, uint16_t *raw_out);

/**
 * Read RTD resistance in ohms.
 */
esp_err_t max31865_read_resistance(max31865_t *rtd, float *r_out);

/**
 * Read temperature in °C (×10 for 0.1°C resolution).
 * Uses Callendar-Van Dusen equation.
 */
int max31865_read_deci_c(max31865_t *rtd);

/**
 * Read fault status register. Non-zero indicates fault.
 */
esp_err_t max31865_read_fault(max31865_t *rtd, uint8_t *fault_out);

/**
 * Clear fault status.
 */
esp_err_t max31865_clear_fault(max31865_t *rtd);
