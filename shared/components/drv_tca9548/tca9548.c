#include "tca9548.h"
#include "esp_log.h"

static const char *TAG = "tca9548";

esp_err_t tca9548_init(tca9548_t *mux, i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &mux->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device @ 0x%02X: %s", addr, esp_err_to_name(ret));
        return ret;
    }
    mux->current_channel = 0xFF;
    return tca9548_select_none(mux);
}

esp_err_t tca9548_select_channel(tca9548_t *mux, uint8_t channel)
{
    if (channel > 7) return ESP_ERR_INVALID_ARG;
    if (channel == mux->current_channel) return ESP_OK;

    uint8_t mask = 1 << channel;
    esp_err_t ret = i2c_master_transmit(mux->dev, &mask, 1, 100);
    if (ret == ESP_OK) {
        mux->current_channel = channel;
    }
    return ret;
}

esp_err_t tca9548_select_none(tca9548_t *mux)
{
    uint8_t mask = 0x00;
    esp_err_t ret = i2c_master_transmit(mux->dev, &mask, 1, 100);
    if (ret == ESP_OK) {
        mux->current_channel = 0xFF;
    }
    return ret;
}
