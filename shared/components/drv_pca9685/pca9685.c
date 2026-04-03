#include "pca9685.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "pca9685";

/* Register addresses */
#define REG_MODE1       0x00
#define REG_MODE2       0x01
#define REG_LED0_ON_L   0x06
#define REG_ALL_ON_L    0xFA
#define REG_ALL_OFF_L   0xFC
#define REG_PRE_SCALE   0xFE

/* MODE1 bits */
#define MODE1_RESTART   (1 << 7)
#define MODE1_SLEEP     (1 << 4)
#define MODE1_AI        (1 << 5)    /* Auto-increment */
#define MODE1_ALLCALL   (1 << 0)

/* MODE2 bits */
#define MODE2_OUTDRV    (1 << 2)    /* Totem-pole output */

/* LED register full-on/off bit */
#define LED_FULL_BIT    (1 << 4)    /* Bit 12 in ON/OFF registers */

static esp_err_t write_reg(pca9685_t *drv, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(drv->dev, buf, 2, 100);
}

static esp_err_t read_reg(pca9685_t *drv, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(drv->dev, &reg, 1, val, 1, 100);
}

esp_err_t pca9685_init(pca9685_t *drv, i2c_master_bus_handle_t bus,
                       uint8_t addr, uint16_t freq_hz)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &drv->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device @ 0x%02X: %s", addr, esp_err_to_name(ret));
        return ret;
    }

    /* Enter sleep mode to set prescaler */
    ret = write_reg(drv, REG_MODE1, MODE1_SLEEP | MODE1_AI | MODE1_ALLCALL);
    if (ret != ESP_OK) return ret;

    /* Set prescaler: freq = 25MHz / (4096 * (prescale + 1)) */
    uint8_t prescale = (uint8_t)((25000000.0 / (4096.0 * freq_hz)) - 1.0 + 0.5);
    if (prescale < 3) prescale = 3;     /* Min prescale */
    ret = write_reg(drv, REG_PRE_SCALE, prescale);
    if (ret != ESP_OK) return ret;

    /* Wake up */
    ret = write_reg(drv, REG_MODE1, MODE1_AI | MODE1_ALLCALL);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(1));   /* Wait for oscillator to stabilize */

    /* Restart and set totem-pole output */
    ret = write_reg(drv, REG_MODE1, MODE1_RESTART | MODE1_AI | MODE1_ALLCALL);
    if (ret != ESP_OK) return ret;
    ret = write_reg(drv, REG_MODE2, MODE2_OUTDRV);
    if (ret != ESP_OK) return ret;

    /* All channels off initially */
    pca9685_all_off(drv);

    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized (prescale=%d, freq≈%d Hz)",
             addr, prescale, (int)(25000000.0 / (4096.0 * (prescale + 1))));
    return ESP_OK;
}

esp_err_t pca9685_set_duty(pca9685_t *drv, uint8_t channel, uint16_t duty)
{
    if (channel > 15) return ESP_ERR_INVALID_ARG;

    uint8_t reg = REG_LED0_ON_L + 4 * channel;

    if (duty == 0) {
        return pca9685_set_full_off(drv, channel);
    }
    if (duty >= 4096) {
        return pca9685_set_full_on(drv, channel);
    }

    /* ON at count 0, OFF at count = duty */
    uint8_t buf[5] = {
        reg,
        0x00,               /* ON_L */
        0x00,               /* ON_H */
        (uint8_t)(duty & 0xFF),       /* OFF_L */
        (uint8_t)((duty >> 8) & 0x0F) /* OFF_H */
    };
    return i2c_master_transmit(drv->dev, buf, 5, 100);
}

esp_err_t pca9685_set_full_on(pca9685_t *drv, uint8_t channel)
{
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    uint8_t reg = REG_LED0_ON_L + 4 * channel;
    uint8_t buf[5] = {reg, 0x00, LED_FULL_BIT, 0x00, 0x00};
    return i2c_master_transmit(drv->dev, buf, 5, 100);
}

esp_err_t pca9685_set_full_off(pca9685_t *drv, uint8_t channel)
{
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    uint8_t reg = REG_LED0_ON_L + 4 * channel;
    uint8_t buf[5] = {reg, 0x00, 0x00, 0x00, LED_FULL_BIT};
    return i2c_master_transmit(drv->dev, buf, 5, 100);
}

esp_err_t pca9685_all_off(pca9685_t *drv)
{
    uint8_t buf[5] = {REG_ALL_OFF_L, 0x00, 0x00, 0x00, LED_FULL_BIT};
    return i2c_master_transmit(drv->dev, buf, 5, 100);
}

esp_err_t pca9685_sleep(pca9685_t *drv, bool sleep)
{
    uint8_t mode1;
    esp_err_t ret = read_reg(drv, REG_MODE1, &mode1);
    if (ret != ESP_OK) return ret;

    if (sleep) {
        mode1 |= MODE1_SLEEP;
    } else {
        mode1 &= ~MODE1_SLEEP;
    }
    return write_reg(drv, REG_MODE1, mode1);
}
