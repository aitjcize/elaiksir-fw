#include "tca9555.h"
#include "esp_log.h"

static const char *TAG = "tca9555";

/* Register addresses */
#define REG_INPUT_P0    0x00
#define REG_INPUT_P1    0x01
#define REG_OUTPUT_P0   0x02
#define REG_OUTPUT_P1   0x03
#define REG_INVERT_P0   0x04
#define REG_INVERT_P1   0x05
#define REG_CONFIG_P0   0x06    /* Direction: 1=input, 0=output */
#define REG_CONFIG_P1   0x07

static esp_err_t write_reg(tca9555_t *gpio, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(gpio->dev, buf, 2, 100);
}

static esp_err_t read_reg(tca9555_t *gpio, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(gpio->dev, &reg, 1, val, 1, 100);
}

esp_err_t tca9555_init(tca9555_t *gpio, i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &gpio->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device @ 0x%02X: %s", addr, esp_err_to_name(ret));
        return ret;
    }

    /* Default: all pins as input */
    write_reg(gpio, REG_CONFIG_P0, 0xFF);
    write_reg(gpio, REG_CONFIG_P1, 0xFF);
    /* No polarity inversion */
    write_reg(gpio, REG_INVERT_P0, 0x00);
    write_reg(gpio, REG_INVERT_P1, 0x00);

    ESP_LOGI(TAG, "TCA9555 @ 0x%02X initialized", addr);
    return ESP_OK;
}

esp_err_t tca9555_set_direction(tca9555_t *gpio, uint8_t port0_dir, uint8_t port1_dir)
{
    esp_err_t ret = write_reg(gpio, REG_CONFIG_P0, port0_dir);
    if (ret != ESP_OK) return ret;
    return write_reg(gpio, REG_CONFIG_P1, port1_dir);
}

esp_err_t tca9555_write(tca9555_t *gpio, uint8_t port0_val, uint8_t port1_val)
{
    esp_err_t ret = write_reg(gpio, REG_OUTPUT_P0, port0_val);
    if (ret != ESP_OK) return ret;
    return write_reg(gpio, REG_OUTPUT_P1, port1_val);
}

esp_err_t tca9555_read(tca9555_t *gpio, uint8_t *port0_val, uint8_t *port1_val)
{
    esp_err_t ret = read_reg(gpio, REG_INPUT_P0, port0_val);
    if (ret != ESP_OK) return ret;
    return read_reg(gpio, REG_INPUT_P1, port1_val);
}

esp_err_t tca9555_write16(tca9555_t *gpio, uint16_t val)
{
    return tca9555_write(gpio, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8));
}

esp_err_t tca9555_read16(tca9555_t *gpio, uint16_t *val)
{
    uint8_t p0, p1;
    esp_err_t ret = tca9555_read(gpio, &p0, &p1);
    if (ret == ESP_OK) {
        *val = ((uint16_t)p1 << 8) | p0;
    }
    return ret;
}
