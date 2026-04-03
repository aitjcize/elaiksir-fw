#pragma once

#include "motor_if.h"
#include "tca9548.h"
#include "pca9685.h"
#include "ads1115.h"

/**
 * Liquid-machine motor backend.
 *
 * Control chain: I2C → TCA9548 mux → PCA9685 PWM → DRV8841 H-bridge.
 * Current sense: I2C → TCA9548 mux → ADS1115 ADC → shunt resistor.
 *
 * Each DRV8841 has two H-bridge channels (A and B).
 * Each channel uses 2 PWM pins (xIN1, xIN2) for direction/speed:
 *   - Forward:  xIN1=PWM, xIN2=LOW
 *   - Reverse:  xIN1=LOW, xIN2=PWM
 *   - Brake:    xIN1=LOW, xIN2=LOW
 */

typedef struct {
    tca9548_t   *pwm_mux;          /* I2C mux for PCA9685 */
    uint8_t      pwm_mux_channel;   /* Mux channel to select */
    pca9685_t   *pwm;              /* PCA9685 controlling this motor */
    uint8_t      pwm_ch_in1;       /* PCA9685 channel for xIN1 */
    uint8_t      pwm_ch_in2;       /* PCA9685 channel for xIN2 */

    tca9548_t   *adc_mux;          /* I2C mux for ADS1115 (can be NULL) */
    uint8_t      adc_mux_channel;
    ads1115_t   *adc;              /* ADS1115 for current sense (can be NULL) */
    ads1115_mux_t adc_channel;     /* ADS1115 mux/channel for this motor */

    int          shunt_mohm;       /* Current sense shunt in mΩ */
} motor_drv8841_cfg_t;

/**
 * Create a motor_t with the DRV8841 H-bridge backend.
 * @param name   Motor name (e.g. "M1", "M5")
 * @param cfg    Configuration — pointers to shared mux/pwm/adc instances
 */
motor_t *motor_drv8841_create(const char *name, const motor_drv8841_cfg_t *cfg);
