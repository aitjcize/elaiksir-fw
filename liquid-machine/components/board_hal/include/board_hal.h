#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"

/**
 * GPIO Pin Definitions — Liquid Machine MM32P4 V1 (ESP32-P4)
 *
 * This board uses I2C-based peripheral expansion extensively:
 *   - 3× I2C buses with isolators and TCA9548 muxes
 *   - PCA9685 PWM controllers → DRV8841 H-bridge motor drivers
 *   - ADS1115 ADCs for motor current sensing
 *   - NAU7802 24-bit ADCs for load cells
 *   - MAX31865 RTD sensors (SPI) for temperature
 */

/* --- Power input: 24V DC via KF25C-7.62-4P connector (P1) --- */

/* --- I2C Bus 1: main peripheral bus (load cells, ADCs, LEDs) --- */
#define BOARD_HAL_I2C1_SDA_PIN          GPIO_NUM_3      /* I2C1 SDA (from ESP32-P4 schematic) */
#define BOARD_HAL_I2C1_SCL_PIN          GPIO_NUM_2      /* I2C1 SCL */

/* --- I2C Bus 2: motor PWM + current sensing --- */
#define BOARD_HAL_I2C2_SDA_PIN          GPIO_NUM_7      /* I2C2 SDA */
#define BOARD_HAL_I2C2_SCL_PIN          GPIO_NUM_6      /* I2C2 SCL */

/* --- I2C Bus 3: GPIO expander (TCA9555) --- */
#define BOARD_HAL_I2C3_SDA_PIN          GPIO_NUM_23     /* I2C3 SDA */
#define BOARD_HAL_I2C3_SCL_PIN          GPIO_NUM_22     /* I2C3 SCL */

/* --- SPI: MAX31865 temperature sensors --- */
#define BOARD_HAL_SPI_MOSI_PIN          GPIO_NUM_11     /* MOSI */
#define BOARD_HAL_SPI_MISO_PIN          GPIO_NUM_13     /* MISO */
#define BOARD_HAL_SPI_SCLK_PIN          GPIO_NUM_12     /* SCK */
#define BOARD_HAL_TEMP1_CS_PIN          GPIO_NUM_10     /* CS1 (PT100 sensor 1) */
#define BOARD_HAL_TEMP2_CS_PIN          GPIO_NUM_9      /* CS2 (PT100 sensor 2) */

/* --- RMII Ethernet (LAN8720A) --- */
#define BOARD_HAL_ETH_MDC_PIN           GPIO_NUM_31     /* RMII MDC */
#define BOARD_HAL_ETH_MDIO_PIN          GPIO_NUM_27     /* RMII MDIO */
#define BOARD_HAL_ETH_TXD0_PIN          GPIO_NUM_34     /* RMII TXD0 */
#define BOARD_HAL_ETH_TXD1_PIN          GPIO_NUM_35     /* RMII TXD1 */
#define BOARD_HAL_ETH_TX_EN_PIN         GPIO_NUM_33     /* RMII TX_EN */
#define BOARD_HAL_ETH_RXD0_PIN          GPIO_NUM_29     /* RMII RXD0 */
#define BOARD_HAL_ETH_RXD1_PIN          GPIO_NUM_28     /* RMII RXD1 */
#define BOARD_HAL_ETH_CLK_PIN           GPIO_NUM_30     /* RMII CLK */
#define BOARD_HAL_ETH_RSTN_PIN          GPIO_NUM_32     /* RMII RSTN */

/* --- USB-UART (CP2102) --- */
#define BOARD_HAL_UART_TXD_PIN          GPIO_NUM_24     /* TXD0 to CP2102 RXD */
#define BOARD_HAL_UART_RXD_PIN          GPIO_NUM_25     /* RXD0 to CP2102 TXD */

/* --- SD Card --- */
#define BOARD_HAL_SD_CMD_PIN            GPIO_NUM_44     /* SD1_CMD */
#define BOARD_HAL_SD_CLK_PIN            GPIO_NUM_43     /* SD1_SCK */
#define BOARD_HAL_SD_D0_PIN             GPIO_NUM_39     /* SD1_D0 */
#define BOARD_HAL_SD_D1_PIN             GPIO_NUM_40     /* SD1_D1 */
#define BOARD_HAL_SD_D2_PIN             GPIO_NUM_41     /* SD1_D2 (DAT2) */
#define BOARD_HAL_SD_D3_PIN             GPIO_NUM_42     /* SD1_D3 (CD/DAT3) */
#define BOARD_HAL_SD_DET_PIN            GPIO_NUM_46     /* SD1_DET card detect */

/* --- Flow sensor --- */
#define BOARD_HAL_FLOW_IN_PIN           GPIO_NUM_21     /* Flow pulse input (via LTV-217-G opto) */
#define BOARD_HAL_FLOW_PWM_PIN          GPIO_NUM_20     /* Flow PWM output */

/* --- ESP32-P4 boot/enable --- */
#define BOARD_HAL_P4_EN_PIN             GPIO_NUM_36     /* Module enable (GPIO36/BOOT_EN) */

/*
 * I2C device addresses (after TCA9548 mux channel selection)
 *
 * I2C1 mux (TCA9548 @ 0x70):
 *   CH0-CH3: NAU7802 load cells (all @ 0x2A, selected by mux)
 *   CH5:     ADS1115 ADC (@ 0x48)
 *   CH6:     ADS1115 ADC (@ 0x48)
 *   CH7:     PCA9685 LED driver (@ 0x40)
 *
 * I2C2 mux (TCA9548 @ 0x70):
 *   CH0:     PCA9685 motor PWM A (@ 0x40)
 *   CH2-CH4: ADS1115 motor current (@ 0x48)
 *   CH5:     PCA9685 motor PWM B (@ 0x40)
 *
 * I2C3 (direct, no mux):
 *   TCA9555 GPIO expander (@ 0x20)
 */
#define BOARD_HAL_I2C_MUX_ADDR         0x70
#define BOARD_HAL_NAU7802_ADDR          0x2A
#define BOARD_HAL_ADS1115_ADDR          0x48
#define BOARD_HAL_PCA9685_ADDR          0x40
#define BOARD_HAL_TCA9555_ADDR          0x20

/* Number of motors / load cells */
#define BOARD_HAL_NUM_MOTORS            10
#define BOARD_HAL_NUM_LOAD_CELLS        4
#define BOARD_HAL_NUM_TEMP_SENSORS      2
#define BOARD_HAL_NUM_SOLENOIDS         6

/* I2C bus handles (shared across drivers) */
extern i2c_master_bus_handle_t i2c1_bus;
extern i2c_master_bus_handle_t i2c2_bus;
extern i2c_master_bus_handle_t i2c3_bus;

/**
 * Initialize board-level hardware: I2C buses, SPI bus, GPIO.
 */
esp_err_t board_hal_init(void);
