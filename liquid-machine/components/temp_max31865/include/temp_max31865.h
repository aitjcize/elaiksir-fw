#pragma once

#include "temp_sensor_if.h"
#include "max31865.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

/**
 * Liquid-machine temperature sensor backend.
 *
 * Wraps MAX31865 SPI RTD sensor as a temp_sensor_t.
 * Supports PT100 (R_ref=430Ω) and PT1000 (R_ref=4300Ω).
 */

/**
 * Create a temp_sensor_t with MAX31865 backend.
 * @param name      Sensor name (e.g. "temp1", "temp2")
 * @param host      SPI host (e.g. SPI2_HOST)
 * @param cs_pin    Chip select GPIO
 * @param r_ref     Reference resistor (430 for PT100, 4300 for PT1000)
 * @param r_nominal RTD nominal at 0°C (100 for PT100, 1000 for PT1000)
 */
temp_sensor_t *temp_sensor_max31865_create(const char *name,
                                            spi_host_device_t host,
                                            gpio_num_t cs_pin,
                                            float r_ref, float r_nominal);
