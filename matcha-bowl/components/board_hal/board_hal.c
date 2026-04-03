#include "board_hal.h"
#include "esp_log.h"

static const char *TAG = "board_hal";

adc_oneshot_unit_handle_t adc1_handle = NULL;

esp_err_t board_hal_init(void)
{
    esp_err_t ret;

    /* --- Digital outputs --- */
    const gpio_num_t output_pins[] = {
        BOARD_HAL_HEAT1_PIN,
        BOARD_HAL_HEAT2_PIN,
        BOARD_HAL_MOTOR_PIN,
        BOARD_HAL_RGB_SW_PIN,
        BOARD_HAL_RGB_PWM_PIN,
        BOARD_HAL_LCD_SW_PIN,
        BOARD_HAL_LCD_DC_PIN,
    };

    gpio_config_t out_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 0,
    };
    for (int i = 0; i < sizeof(output_pins) / sizeof(output_pins[0]); i++) {
        out_conf.pin_bit_mask |= (1ULL << output_pins[i]);
    }
    ret = gpio_config(&out_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure output GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Ensure outputs start low */
    for (int i = 0; i < sizeof(output_pins) / sizeof(output_pins[0]); i++) {
        gpio_set_level(output_pins[i], 0);
    }

    /* --- Digital inputs --- */
    gpio_config_t in_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL << BOARD_HAL_ZERO_CROSS_PIN) |
                        (1ULL << BOARD_HAL_ZERO_CROSS_SW_PIN) |
                        (1ULL << BOARD_HAL_KS_PIN) |
                        (1ULL << BOARD_HAL_TOUCH_SW1_PIN) |
                        (1ULL << BOARD_HAL_TOUCH_SW2_PIN) |
                        (1ULL << BOARD_HAL_TOUCH_SW3_PIN) |
                        (1ULL << BOARD_HAL_TOUCH_SW4_PIN),
    };
    ret = gpio_config(&in_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure input GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }

    /* --- ADC1 (shared by NTC and motor current sense) --- */
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&adc_cfg, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC1: %s", esp_err_to_name(ret));
        return ret;
    }

    /* NTC channel — 11dB attenuation for full 0–3.3V range */
    adc_oneshot_chan_cfg_t ntc_chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(adc1_handle, BOARD_HAL_NTC_ADC_CHAN, &ntc_chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure NTC ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Motor current sense channel */
    adc_oneshot_chan_cfg_t cs_chan_cfg = {
        .atten = ADC_ATTEN_DB_6,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(adc1_handle, BOARD_HAL_MOTOR_CS_ADC_CHAN, &cs_chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure motor CS ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Board HAL initialized");
    return ESP_OK;
}
