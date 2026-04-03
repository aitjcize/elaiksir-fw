#include "rgb_led.h"
#include "board_hal.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "rgb_led";

#define RGB_LEDC_TIMER      LEDC_TIMER_1
#define RGB_LEDC_CHANNEL    LEDC_CHANNEL_1
#define RGB_LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#define RGB_PWM_FREQ_HZ     1000
#define RGB_PWM_RESOLUTION  LEDC_TIMER_10_BIT
#define RGB_PWM_MAX_DUTY    ((1 << 10) - 1)

esp_err_t rgb_led_init(void)
{
    esp_err_t ret;

    ledc_timer_config_t timer_conf = {
        .speed_mode = RGB_LEDC_SPEED_MODE,
        .duty_resolution = RGB_PWM_RESOLUTION,
        .timer_num = RGB_LEDC_TIMER,
        .freq_hz = RGB_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) return ret;

    ledc_channel_config_t chan_conf = {
        .speed_mode = RGB_LEDC_SPEED_MODE,
        .channel = RGB_LEDC_CHANNEL,
        .timer_sel = RGB_LEDC_TIMER,
        .gpio_num = BOARD_HAL_RGB_PWM_PIN,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&chan_conf);
    if (ret != ESP_OK) return ret;

    /* Start with LED off */
    gpio_set_level(BOARD_HAL_RGB_SW_PIN, 0);

    ESP_LOGI(TAG, "RGB LED initialized");
    return ESP_OK;
}

void rgb_led_enable(bool on)
{
    gpio_set_level(BOARD_HAL_RGB_SW_PIN, on ? 1 : 0);
    ESP_LOGI(TAG, "RGB LED power %s", on ? "ON" : "OFF");
}

void rgb_led_set_brightness(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    uint32_t duty = (uint32_t)RGB_PWM_MAX_DUTY * percent / 100;
    ledc_set_duty(RGB_LEDC_SPEED_MODE, RGB_LEDC_CHANNEL, duty);
    ledc_update_duty(RGB_LEDC_SPEED_MODE, RGB_LEDC_CHANNEL);
}
