#include "display.h"
#include "board_hal.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>

static const char *TAG = "display";

static spi_device_handle_t spi_dev = NULL;

esp_err_t display_init(void)
{
    esp_err_t ret;

    /* Power on the OLED via P-FET (active low for P-channel) */
    gpio_set_level(BOARD_HAL_LCD_SW_PIN, 1);

    /* Initialize SPI bus */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_HAL_LCD_MOSI_PIN,
        .sclk_io_num = BOARD_HAL_LCD_SCLK_PIN,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024,
    };
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 4 * 1000 * 1000,     /* 4 MHz */
        .mode = 0,
        .spics_io_num = -1,                     /* No CS — directly connected */
        .queue_size = 8,
    };
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /*
     * TODO: Send OLED initialization sequence here.
     * This depends on the specific OLED controller (SSD1306, SH1106, etc).
     * Typical sequence: display off, set mux ratio, set display offset,
     * set start line, set segment remap, set COM scan direction,
     * set COM pins, set contrast, display on.
     */

    ESP_LOGI(TAG, "Display SPI initialized (stub — add OLED init sequence)");
    return ESP_OK;
}

void display_power(bool on)
{
    /* P-channel MOSFET: low = on, high = off */
    gpio_set_level(BOARD_HAL_LCD_SW_PIN, on ? 1 : 0);
    ESP_LOGI(TAG, "Display power %s", on ? "ON" : "OFF");
}

void display_clear(void)
{
    /* TODO: clear OLED framebuffer */
    ESP_LOGD(TAG, "Display clear (stub)");
}

void display_show_text(int line, const char *text)
{
    /* TODO: render text to OLED */
    ESP_LOGI(TAG, "Display L%d: %s (stub)", line, text);
}

void display_show_temperature(int deci_c)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "Temp: %d.%d C", deci_c / 10, deci_c % 10);
    display_show_text(0, buf);
}

void display_show_status(const char *status)
{
    display_show_text(1, status);
}
