#include "touch.h"
#include "board_hal.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

static const gpio_num_t btn_pins[TOUCH_BTN_COUNT] = {
    [TOUCH_BTN_SW1] = BOARD_HAL_TOUCH_SW1_PIN,
    [TOUCH_BTN_SW2] = BOARD_HAL_TOUCH_SW2_PIN,
    [TOUCH_BTN_SW3] = BOARD_HAL_TOUCH_SW3_PIN,
    [TOUCH_BTN_SW4] = BOARD_HAL_TOUCH_SW4_PIN,
};

static touch_event_cb_t event_cb = NULL;
static uint8_t prev_state = 0;

#define TOUCH_POLL_INTERVAL_MS  50
#define TOUCH_DEBOUNCE_COUNT    2

static uint8_t debounce_cnt[TOUCH_BTN_COUNT] = {0};
static uint8_t debounced_state = 0;

static void touch_poll_task(void *arg)
{
    while (1) {
        uint8_t raw = 0;
        for (int i = 0; i < TOUCH_BTN_COUNT; i++) {
            if (gpio_get_level(btn_pins[i])) {
                raw |= (1 << i);
            }
        }

        /* Debounce each button */
        for (int i = 0; i < TOUCH_BTN_COUNT; i++) {
            bool raw_pressed = (raw >> i) & 1;
            bool deb_pressed = (debounced_state >> i) & 1;

            if (raw_pressed != deb_pressed) {
                debounce_cnt[i]++;
                if (debounce_cnt[i] >= TOUCH_DEBOUNCE_COUNT) {
                    /* State change confirmed */
                    if (raw_pressed) {
                        debounced_state |= (1 << i);
                    } else {
                        debounced_state &= ~(1 << i);
                    }
                    debounce_cnt[i] = 0;

                    if (event_cb) {
                        event_cb((touch_btn_t)i, raw_pressed);
                    }
                }
            } else {
                debounce_cnt[i] = 0;
            }
        }

        prev_state = debounced_state;
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_INTERVAL_MS));
    }
}

esp_err_t touch_init(void)
{
    /* GPIO already configured as input in board_hal_init() */
    xTaskCreate(touch_poll_task, "touch_poll", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Touch buttons initialized (polling %d ms)", TOUCH_POLL_INTERVAL_MS);
    return ESP_OK;
}

void touch_register_callback(touch_event_cb_t cb)
{
    event_cb = cb;
}

bool touch_is_pressed(touch_btn_t btn)
{
    if (btn >= TOUCH_BTN_COUNT) return false;
    return (debounced_state >> btn) & 1;
}

uint8_t touch_read_all(void)
{
    return debounced_state;
}
