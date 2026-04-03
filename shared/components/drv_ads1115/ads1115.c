#include "ads1115.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ads1115";

/* Register addresses */
#define REG_CONVERSION  0x00
#define REG_CONFIG      0x01

/* Config register bits */
#define CFG_OS_START    (1 << 15)   /* Start single conversion */
#define CFG_OS_BUSY     (0 << 15)   /* Conversion in progress */
#define CFG_OS_IDLE     (1 << 15)   /* Not performing conversion */
#define CFG_MODE_SINGLE (1 << 8)    /* Single-shot mode */

/* Full-scale voltage in µV for each PGA setting */
static const int pga_fs_uv[] = {
    6144000, 4096000, 2048000, 1024000, 512000, 256000
};

static esp_err_t write_reg16(ads1115_t *adc, uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = {reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    return i2c_master_transmit(adc->dev, buf, 3, 100);
}

static esp_err_t read_reg16(ads1115_t *adc, uint8_t reg, uint16_t *val)
{
    uint8_t rx[2];
    esp_err_t ret = i2c_master_transmit_receive(adc->dev, &reg, 1, rx, 2, 100);
    if (ret == ESP_OK) {
        *val = ((uint16_t)rx[0] << 8) | rx[1];
    }
    return ret;
}

esp_err_t ads1115_init(ads1115_t *adc, i2c_master_bus_handle_t bus,
                       uint8_t addr, ads1115_pga_t pga, ads1115_sps_t sps)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &adc->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device @ 0x%02X: %s", addr, esp_err_to_name(ret));
        return ret;
    }
    adc->pga = pga;
    adc->sps = sps;

    ESP_LOGI(TAG, "ADS1115 @ 0x%02X initialized (PGA=%d, SPS=%d)", addr, pga, sps);
    return ESP_OK;
}

esp_err_t ads1115_read_raw(ads1115_t *adc, ads1115_mux_t mux, int16_t *raw_out)
{
    /* Build config: OS=start, MUX, PGA, MODE=single, DR, COMP_QUE=disable */
    uint16_t config = CFG_OS_START |
                      ((uint16_t)mux << 12) |
                      ((uint16_t)adc->pga << 9) |
                      CFG_MODE_SINGLE |
                      ((uint16_t)adc->sps << 5) |
                      0x03;  /* Disable comparator */

    esp_err_t ret = write_reg16(adc, REG_CONFIG, config);
    if (ret != ESP_OK) return ret;

    /* Wait for conversion — delay depends on SPS setting */
    static const int sps_delay_ms[] = {130, 65, 35, 18, 10, 6, 4, 3};
    vTaskDelay(pdMS_TO_TICKS(sps_delay_ms[adc->sps] + 1));

    /* Poll for completion */
    for (int i = 0; i < 10; i++) {
        uint16_t status;
        ret = read_reg16(adc, REG_CONFIG, &status);
        if (ret != ESP_OK) return ret;
        if (status & CFG_OS_IDLE) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Read conversion result */
    uint16_t raw;
    ret = read_reg16(adc, REG_CONVERSION, &raw);
    if (ret == ESP_OK) {
        *raw_out = (int16_t)raw;
    }
    return ret;
}

int ads1115_raw_to_mv(ads1115_t *adc, int16_t raw)
{
    /* mv = raw * FS_mV / 32768 */
    int fs_mv = pga_fs_uv[adc->pga] / 1000;
    return (int)((int32_t)raw * fs_mv / 32768);
}
