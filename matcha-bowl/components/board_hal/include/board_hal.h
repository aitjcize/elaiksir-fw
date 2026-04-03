#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/*
 * GPIO Pin Definitions — Matcha Machine Rev 0227
 *
 * Two-board system: Power Board (CN3) ↔ ESP32 Board (CN6) via 12-pin ribbon.
 */

/* Heater control */
#define BOARD_HAL_HEAT1_PIN             GPIO_NUM_26     /* Relay on/off */
#define BOARD_HAL_HEAT2_PIN             GPIO_NUM_19     /* TRIAC phase-angle */

/* AC zero-crossing detection */
#define BOARD_HAL_ZERO_CROSS_PIN        GPIO_NUM_36     /* Primary (PC817, input-only) */
#define BOARD_HAL_ZERO_CROSS_SW_PIN     GPIO_NUM_13     /* Alternative (TD3062) */

/* Motor */
#define BOARD_HAL_MOTOR_PIN             GPIO_NUM_23     /* MOSFET gate (PWM) */
#define BOARD_HAL_MOTOR_CS_PIN          GPIO_NUM_34     /* Current sense ADC (input-only) */
#define BOARD_HAL_MOTOR_CS_ADC_CHAN      ADC_CHANNEL_6   /* GPIO34 = ADC1_CH6 */

/* NTC temperature sensor */
#define BOARD_HAL_NTC_PIN               GPIO_NUM_39     /* ADC input (input-only) */
#define BOARD_HAL_NTC_ADC_CHAN           ADC_CHANNEL_3   /* GPIO39 = ADC1_CH3 */

/* External switch (lid/bowl) */
#define BOARD_HAL_KS_PIN                GPIO_NUM_15

/* RGB LED */
#define BOARD_HAL_RGB_SW_PIN            GPIO_NUM_27     /* Power MOSFET gate */
#define BOARD_HAL_RGB_PWM_PIN           GPIO_NUM_14     /* Brightness PWM */

/* OLED display (SPI) */
#define BOARD_HAL_LCD_SW_PIN            GPIO_NUM_4      /* Power MOSFET gate */
#define BOARD_HAL_LCD_SCLK_PIN          GPIO_NUM_5      /* SPI clock */
#define BOARD_HAL_LCD_MOSI_PIN          GPIO_NUM_17     /* SPI data */
#define BOARD_HAL_LCD_DC_PIN            GPIO_NUM_16     /* Data/Command select */

/* Touch buttons (TTP223 direct GPIO) */
#define BOARD_HAL_TOUCH_SW1_PIN         GPIO_NUM_35     /* Input-only */
#define BOARD_HAL_TOUCH_SW2_PIN         GPIO_NUM_32
#define BOARD_HAL_TOUCH_SW3_PIN         GPIO_NUM_33
#define BOARD_HAL_TOUCH_SW4_PIN         GPIO_NUM_25

/* I2C — CDS8712 touch controller (alternative scheme) */
#define BOARD_HAL_I2C_SDA_PIN           GPIO_NUM_21
#define BOARD_HAL_I2C_SCL_PIN           GPIO_NUM_22

/* NTC parameters (10K NTC, B=3950, voltage divider R25=10K to 3.3V) */
#define BOARD_HAL_NTC_B_PARAM           3950
#define BOARD_HAL_NTC_R_REF             10000   /* NTC resistance at T_REF */
#define BOARD_HAL_NTC_T_REF             25      /* Reference temp in °C */
#define BOARD_HAL_NTC_R_SERIES          10000   /* Series resistor (R25) */

/* Motor current sense: 0.005Ω shunt (R34), 200Ω series filter (R33) */
#define BOARD_HAL_MOTOR_SHUNT_MOHM      5       /* Shunt resistance in mΩ */

/* ADC handle shared across drivers */
extern adc_oneshot_unit_handle_t adc1_handle;

/**
 * Initialize all board-level GPIO and shared peripherals (ADC1).
 */
esp_err_t board_hal_init(void);
