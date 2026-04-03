#include "board_hal.h"
#include "esp_log.h"

static const char *TAG = "board_hal";

i2c_master_bus_handle_t i2c1_bus = NULL;
i2c_master_bus_handle_t i2c2_bus = NULL;
i2c_master_bus_handle_t i2c3_bus = NULL;

static esp_err_t init_i2c_bus(int port, gpio_num_t sda, gpio_num_t scl,
                               i2c_master_bus_handle_t *out_handle)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, out_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C port %d init failed: %s", port, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t board_hal_init(void)
{
    esp_err_t ret;

    /* I2C Bus 1: load cells, ADCs, LED driver */
    ret = init_i2c_bus(0, BOARD_HAL_I2C1_SDA_PIN, BOARD_HAL_I2C1_SCL_PIN, &i2c1_bus);
    if (ret != ESP_OK) return ret;

    /* I2C Bus 2: motor PWM, motor current sense */
    ret = init_i2c_bus(1, BOARD_HAL_I2C2_SDA_PIN, BOARD_HAL_I2C2_SCL_PIN, &i2c2_bus);
    if (ret != ESP_OK) return ret;

    /* I2C Bus 3: GPIO expander */
    ret = init_i2c_bus(2, BOARD_HAL_I2C3_SDA_PIN, BOARD_HAL_I2C3_SCL_PIN, &i2c3_bus);
    if (ret != ESP_OK) return ret;

    /* TODO: Initialize SPI bus for MAX31865 temperature sensors */
    /* TODO: Initialize RMII for Ethernet (LAN8720A) */

    ESP_LOGI(TAG, "Board HAL initialized (3 I2C buses)");
    return ESP_OK;
}
