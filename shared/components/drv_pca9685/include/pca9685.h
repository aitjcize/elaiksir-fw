#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * PCA9685 — 16-channel, 12-bit PWM controller.
 *
 * Used for motor PWM (→ DRV8841) and LED brightness control (→ ULN2803A).
 * Each channel has independent 12-bit duty cycle (0–4095).
 * Shared prescaler sets PWM frequency for all channels.
 */

typedef struct {
    i2c_master_dev_handle_t dev;
} pca9685_t;

/**
 * Initialize PCA9685.
 * @param drv       Output: driver instance
 * @param bus       I2C master bus handle
 * @param addr      7-bit address (typically 0x40–0x7F)
 * @param freq_hz   PWM frequency in Hz (24–1526 Hz)
 */
esp_err_t pca9685_init(pca9685_t *drv, i2c_master_bus_handle_t bus,
                       uint8_t addr, uint16_t freq_hz);

/**
 * Set PWM duty for a single channel.
 * @param channel   0–15
 * @param duty      0–4095 (12-bit), or 4096 for full-on
 */
esp_err_t pca9685_set_duty(pca9685_t *drv, uint8_t channel, uint16_t duty);

/**
 * Set a channel fully on or fully off.
 */
esp_err_t pca9685_set_full_on(pca9685_t *drv, uint8_t channel);
esp_err_t pca9685_set_full_off(pca9685_t *drv, uint8_t channel);

/**
 * Turn all channels off.
 */
esp_err_t pca9685_all_off(pca9685_t *drv);

/**
 * Put device to sleep / wake up.
 */
esp_err_t pca9685_sleep(pca9685_t *drv, bool sleep);
