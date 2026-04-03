#include "nau7802.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "nau7802";

/* Register map */
#define REG_PU_CTRL     0x00
#define REG_CTRL1       0x01
#define REG_CTRL2       0x02
#define REG_ADC_B2      0x12    /* MSB */
#define REG_ADC_B1      0x13
#define REG_ADC_B0      0x14    /* LSB */
#define REG_POWER       0x1C

/* PU_CTRL bits */
#define PU_RR           (1 << 0)    /* Register reset */
#define PU_PUD          (1 << 1)    /* Power-up digital */
#define PU_PUA          (1 << 2)    /* Power-up analog */
#define PU_PUR          (1 << 3)    /* Power-up ready (read-only) */
#define PU_CS           (1 << 4)    /* Cycle start */
#define PU_CR           (1 << 5)    /* Cycle ready (data available) */
#define PU_AVDDS        (1 << 7)    /* AVDD source select */

/* CTRL2 bits */
#define CTRL2_CALMOD_OFFSET 0x00
#define CTRL2_CALS          (1 << 2)    /* Start calibration */
#define CTRL2_CAL_ERR       (1 << 3)    /* Calibration error */

static esp_err_t write_reg(nau7802_t *adc, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(adc->dev, buf, 2, 100);
}

static esp_err_t read_reg(nau7802_t *adc, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(adc->dev, &reg, 1, val, 1, 100);
}

static esp_err_t set_bits(nau7802_t *adc, uint8_t reg, uint8_t mask)
{
    uint8_t val;
    esp_err_t ret = read_reg(adc, reg, &val);
    if (ret != ESP_OK) return ret;
    return write_reg(adc, reg, val | mask);
}

static esp_err_t clear_bits(nau7802_t *adc, uint8_t reg, uint8_t mask)
{
    uint8_t val;
    esp_err_t ret = read_reg(adc, reg, &val);
    if (ret != ESP_OK) return ret;
    return write_reg(adc, reg, val & ~mask);
}

esp_err_t nau7802_init(nau7802_t *adc, i2c_master_bus_handle_t bus,
                       nau7802_gain_t gain, nau7802_sps_t sps)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x2A,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &adc->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %s", esp_err_to_name(ret));
        return ret;
    }
    adc->gain = gain;

    /* Reset registers */
    ret = write_reg(adc, REG_PU_CTRL, PU_RR);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(1));
    ret = write_reg(adc, REG_PU_CTRL, 0);
    if (ret != ESP_OK) return ret;

    /* Power up digital + analog */
    ret = write_reg(adc, REG_PU_CTRL, PU_PUD | PU_PUA);
    if (ret != ESP_OK) return ret;

    /* Wait for power-up ready */
    for (int i = 0; i < 100; i++) {
        uint8_t pu;
        ret = read_reg(adc, REG_PU_CTRL, &pu);
        if (ret != ESP_OK) return ret;
        if (pu & PU_PUR) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Set gain in CTRL1[2:0] */
    uint8_t ctrl1 = (uint8_t)gain & 0x07;
    ret = write_reg(adc, REG_CTRL1, ctrl1);
    if (ret != ESP_OK) return ret;

    /* Set sample rate in CTRL2[6:4] */
    uint8_t ctrl2 = ((uint8_t)sps & 0x07) << 4;
    ret = write_reg(adc, REG_CTRL2, ctrl2);
    if (ret != ESP_OK) return ret;

    /* Use internal LDO, set AVDDS */
    ret = set_bits(adc, REG_PU_CTRL, PU_AVDDS);
    if (ret != ESP_OK) return ret;

    /* Enable cycle start for continuous conversion */
    ret = set_bits(adc, REG_PU_CTRL, PU_CS);
    if (ret != ESP_OK) return ret;

    /* Perform initial offset calibration */
    ret = nau7802_calibrate(adc);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Initial calibration failed");
    }

    ESP_LOGI(TAG, "NAU7802 initialized (gain=%d, sps=%d)", gain, sps);
    return ESP_OK;
}

esp_err_t nau7802_select_channel(nau7802_t *adc, nau7802_channel_t ch)
{
    uint8_t ctrl1;
    esp_err_t ret = read_reg(adc, REG_CTRL1, &ctrl1);
    if (ret != ESP_OK) return ret;

    ctrl1 = (ctrl1 & ~(1 << 7)) | ((uint8_t)ch << 7);
    return write_reg(adc, REG_CTRL1, ctrl1);
}

bool nau7802_data_ready(nau7802_t *adc)
{
    uint8_t pu;
    if (read_reg(adc, REG_PU_CTRL, &pu) != ESP_OK) return false;
    return (pu & PU_CR) != 0;
}

esp_err_t nau7802_read_raw(nau7802_t *adc, int32_t *raw_out)
{
    /* Wait for data ready */
    for (int i = 0; i < 200; i++) {
        if (nau7802_data_ready(adc)) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!nau7802_data_ready(adc)) {
        return ESP_ERR_TIMEOUT;
    }

    /* Read 3 bytes (MSB first) */
    uint8_t b2, b1, b0;
    esp_err_t ret;
    ret = read_reg(adc, REG_ADC_B2, &b2);
    if (ret != ESP_OK) return ret;
    ret = read_reg(adc, REG_ADC_B1, &b1);
    if (ret != ESP_OK) return ret;
    ret = read_reg(adc, REG_ADC_B0, &b0);
    if (ret != ESP_OK) return ret;

    /* Assemble 24-bit signed value */
    uint32_t raw = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
    /* Sign-extend from 24-bit */
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }
    *raw_out = (int32_t)raw;
    return ESP_OK;
}

esp_err_t nau7802_calibrate(nau7802_t *adc)
{
    /* Start internal offset calibration */
    esp_err_t ret = write_reg(adc, REG_CTRL2, CTRL2_CALMOD_OFFSET | CTRL2_CALS);
    if (ret != ESP_OK) return ret;

    /* Wait for calibration to complete (CALS bit clears) */
    for (int i = 0; i < 200; i++) {
        uint8_t ctrl2;
        ret = read_reg(adc, REG_CTRL2, &ctrl2);
        if (ret != ESP_OK) return ret;
        if (!(ctrl2 & CTRL2_CALS)) {
            if (ctrl2 & CTRL2_CAL_ERR) {
                ESP_LOGE(TAG, "Calibration error");
                return ESP_FAIL;
            }
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return ESP_ERR_TIMEOUT;
}
