#include "max31865.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "max31865";

/* Register addresses (read: addr, write: addr | 0x80) */
#define REG_CONFIG      0x00
#define REG_RTD_MSB     0x01
#define REG_RTD_LSB     0x02
#define REG_HFAULT_MSB  0x03
#define REG_HFAULT_LSB  0x04
#define REG_LFAULT_MSB  0x05
#define REG_LFAULT_LSB  0x06
#define REG_FAULT       0x07

/* Config bits */
#define CFG_VBIAS       (1 << 7)
#define CFG_AUTO        (1 << 6)    /* Auto conversion mode */
#define CFG_1SHOT       (1 << 5)
#define CFG_3WIRE       (1 << 4)
#define CFG_FAULT_CLR   (1 << 1)
#define CFG_50HZ        (1 << 0)    /* 50Hz filter (vs 60Hz) */

/* Callendar-Van Dusen constants for PT RTDs */
#define CVD_A  3.9083e-3f
#define CVD_B -5.775e-7f

static esp_err_t write_reg(max31865_t *rtd, uint8_t reg, uint8_t val)
{
    spi_transaction_t txn = {
        .length = 16,
        .tx_data = {(uint8_t)(reg | 0x80), val},
        .flags = SPI_TRANS_USE_TXDATA,
    };
    return spi_device_transmit(rtd->spi, &txn);
}

static esp_err_t read_reg(max31865_t *rtd, uint8_t reg, uint8_t *val)
{
    spi_transaction_t txn = {
        .length = 16,
        .tx_data = {reg, 0xFF},
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    esp_err_t ret = spi_device_transmit(rtd->spi, &txn);
    if (ret == ESP_OK) {
        *val = txn.rx_data[1];
    }
    return ret;
}

esp_err_t max31865_init(max31865_t *rtd, spi_host_device_t host, gpio_num_t cs_pin,
                        float r_ref, float r_nominal, max31865_wire_t wire_mode)
{
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,     /* 1 MHz */
        .mode = 1,                               /* CPOL=0, CPHA=1 */
        .spics_io_num = cs_pin,
        .queue_size = 4,
        .command_bits = 0,
        .address_bits = 0,
    };
    esp_err_t ret = spi_bus_add_device(host, &dev_cfg, &rtd->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return ret;
    }
    rtd->r_ref = r_ref;
    rtd->r_nominal = r_nominal;

    /* Configure: Vbias on, auto conversion, 50Hz filter */
    uint8_t config = CFG_VBIAS | CFG_AUTO | CFG_50HZ;
    if (wire_mode == MAX31865_WIRE_3) {
        config |= CFG_3WIRE;
    }
    ret = write_reg(rtd, REG_CONFIG, config);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "MAX31865 initialized (R_ref=%.0f, R_nom=%.0f, %d-wire)",
             r_ref, r_nominal, wire_mode == MAX31865_WIRE_3 ? 3 : 2);
    return ESP_OK;
}

esp_err_t max31865_read_raw(max31865_t *rtd, uint16_t *raw_out)
{
    uint8_t msb, lsb;
    esp_err_t ret = read_reg(rtd, REG_RTD_MSB, &msb);
    if (ret != ESP_OK) return ret;
    ret = read_reg(rtd, REG_RTD_LSB, &lsb);
    if (ret != ESP_OK) return ret;

    uint16_t raw = ((uint16_t)msb << 8) | lsb;

    /* Check fault bit (bit 0 of LSB) */
    if (raw & 0x01) {
        ESP_LOGW(TAG, "RTD fault detected");
    }

    /* 15-bit ratio is bits [15:1] */
    *raw_out = raw >> 1;
    return ESP_OK;
}

esp_err_t max31865_read_resistance(max31865_t *rtd, float *r_out)
{
    uint16_t raw;
    esp_err_t ret = max31865_read_raw(rtd, &raw);
    if (ret != ESP_OK) return ret;

    /* R_rtd = raw * R_ref / 32768 */
    *r_out = (float)raw * rtd->r_ref / 32768.0f;
    return ESP_OK;
}

int max31865_read_deci_c(max31865_t *rtd)
{
    float r_rtd;
    if (max31865_read_resistance(rtd, &r_rtd) != ESP_OK) {
        return -9999;
    }

    /*
     * Callendar-Van Dusen for T > 0°C:
     *   R = R0 * (1 + A*T + B*T²)
     * Solving quadratic: T = (-A + √(A² - 4B(1 - R/R0))) / 2B
     */
    float z1 = -CVD_A;
    float z2 = CVD_A * CVD_A - 4.0f * CVD_B;
    float z3 = 4.0f * CVD_B / rtd->r_nominal;
    float z4 = 2.0f * CVD_B;

    float temp = (z1 + sqrtf(z2 + z3 * (r_rtd - rtd->r_nominal))) / z4;

    /* For sub-zero temperatures, a more complex equation is needed.
     * This approximation works well for 0–850°C range. */
    if (temp < -50.0f || temp > 900.0f) {
        ESP_LOGW(TAG, "Temperature out of range: %.1f C", temp);
    }

    return (int)(temp * 10.0f);
}

esp_err_t max31865_read_fault(max31865_t *rtd, uint8_t *fault_out)
{
    return read_reg(rtd, REG_FAULT, fault_out);
}

esp_err_t max31865_clear_fault(max31865_t *rtd)
{
    uint8_t config;
    esp_err_t ret = read_reg(rtd, REG_CONFIG, &config);
    if (ret != ESP_OK) return ret;
    return write_reg(rtd, REG_CONFIG, config | CFG_FAULT_CLR);
}
