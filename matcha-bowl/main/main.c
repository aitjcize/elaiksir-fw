#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board_hal.h"
#include "heater.h"         /* matcha backend — heater_matcha_create() */
#include "motor.h"          /* matcha backend — motor_matcha_create() */
#include "temp_sensor.h"    /* matcha backend — temp_sensor_ntc_create() */
#include "heater_if.h"
#include "motor_if.h"
#include "temp_sensor_if.h"
#include "touch.h"
#include "rgb_led.h"
#include "display.h"
#include "cli.h"

static const char *TAG = "matcha";

/*
 * Matcha machine state machine.
 *
 * IDLE     → waiting for user to press start
 * HEATING  → heating water to target temperature
 * MIXING   → motor spinning to whisk matcha
 * DONE     → process complete, waiting for user acknowledgement
 * ERROR    → fault condition (over-temp, motor stall, etc)
 */
typedef enum {
    STATE_IDLE,
    STATE_HEATING,
    STATE_MIXING,
    STATE_DONE,
    STATE_ERROR,
} machine_state_t;

/* --- Configuration --- */
#define TARGET_TEMP_DECI_C      800     /* 80.0°C target water temperature */
#define MAX_TEMP_DECI_C         950     /* 95.0°C safety cutoff */
#define MIXING_SPEED_PERCENT    70      /* Motor speed during mixing */
#define MIXING_DURATION_MS      30000   /* 30 seconds mixing time */
#define MOTOR_MAX_CURRENT_MA    3000    /* Over-current protection threshold */
#define STATUS_POLL_INTERVAL_MS 500

/* Peripheral instances — created by product-specific backends */
static heater_t      *heater;
static motor_t       *whisk_motor;
static temp_sensor_t *water_temp;

static machine_state_t state = STATE_IDLE;
static TickType_t state_enter_tick = 0;

static void enter_state(machine_state_t new_state)
{
    state = new_state;
    state_enter_tick = xTaskGetTickCount();

    switch (new_state) {
    case STATE_IDLE:
        heater_shutdown(heater);
        motor_stop(whisk_motor);
        rgb_led_set_brightness(20);
        display_show_status("Ready");
        ESP_LOGI(TAG, "State: IDLE");
        break;

    case STATE_HEATING:
        heater_set_power(heater, 80);
        rgb_led_set_brightness(50);
        display_show_status("Heating...");
        ESP_LOGI(TAG, "State: HEATING (target=%d.%d C)",
                 TARGET_TEMP_DECI_C / 10, TARGET_TEMP_DECI_C % 10);
        break;

    case STATE_MIXING:
        heater_shutdown(heater);
        motor_set_speed(whisk_motor, MIXING_SPEED_PERCENT);
        rgb_led_set_brightness(80);
        display_show_status("Mixing...");
        ESP_LOGI(TAG, "State: MIXING (%d%% speed, %d s)",
                 MIXING_SPEED_PERCENT, MIXING_DURATION_MS / 1000);
        break;

    case STATE_DONE:
        motor_stop(whisk_motor);
        heater_shutdown(heater);
        rgb_led_set_brightness(100);
        display_show_status("Done!");
        ESP_LOGI(TAG, "State: DONE");
        break;

    case STATE_ERROR:
        heater_shutdown(heater);
        motor_stop(whisk_motor);
        rgb_led_enable(false);
        display_show_status("ERROR!");
        ESP_LOGE(TAG, "State: ERROR");
        break;
    }
}

static uint32_t ms_in_state(void)
{
    return (xTaskGetTickCount() - state_enter_tick) * portTICK_PERIOD_MS;
}

static void touch_event_handler(touch_btn_t btn, bool pressed)
{
    if (!pressed) return;
    ESP_LOGI(TAG, "Touch: SW%d pressed (state=%d)", btn + 1, state);

    switch (btn) {
    case TOUCH_BTN_SW1:
        if (state == STATE_IDLE) {
            enter_state(STATE_HEATING);
        } else if (state == STATE_DONE || state == STATE_ERROR) {
            enter_state(STATE_IDLE);
        } else {
            enter_state(STATE_IDLE);
        }
        break;
    default:
        break;
    }
}

static void control_loop_task(void *arg)
{
    while (1) {
        int temp = temp_sensor_read_deci_c(water_temp);
        if (temp != -9999) {
            display_show_temperature(temp);
        }

        switch (state) {
        case STATE_IDLE:
            break;

        case STATE_HEATING:
            if (temp == -9999) {
                ESP_LOGE(TAG, "Temperature sensor failure");
                enter_state(STATE_ERROR);
                break;
            }
            if (temp >= MAX_TEMP_DECI_C) {
                ESP_LOGE(TAG, "Over-temperature! %d.%d C", temp / 10, temp % 10);
                enter_state(STATE_ERROR);
                break;
            }
            if (temp >= TARGET_TEMP_DECI_C) {
                ESP_LOGI(TAG, "Target temperature reached: %d.%d C",
                         temp / 10, temp % 10);
                enter_state(STATE_MIXING);
                break;
            }

            /* Proportional power control near target */
            {
                int delta = TARGET_TEMP_DECI_C - temp;
                heater_set_power(heater, delta < 50 ? 30 : 80);
            }
            break;

        case STATE_MIXING:
            {
                int current_ma = motor_read_current_ma(whisk_motor);
                if (current_ma > MOTOR_MAX_CURRENT_MA) {
                    ESP_LOGE(TAG, "Motor over-current: %d mA", current_ma);
                    enter_state(STATE_ERROR);
                    break;
                }
            }
            if (ms_in_state() >= MIXING_DURATION_MS) {
                enter_state(STATE_DONE);
            }
            break;

        case STATE_DONE:
        case STATE_ERROR:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_POLL_INTERVAL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Matcha Machine firmware starting...");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize board-level hardware */
    ESP_ERROR_CHECK(board_hal_init());

    /* Create peripheral instances from matcha-bowl backends */
    heater      = heater_matcha_create();
    whisk_motor = motor_matcha_create();
    water_temp  = temp_sensor_ntc_create();

    /* Initialize via abstract interfaces */
    ESP_ERROR_CHECK(heater_init(heater));
    ESP_ERROR_CHECK(motor_init(whisk_motor));
    ESP_ERROR_CHECK(temp_sensor_init(water_temp));

    /* Initialize matcha-specific peripherals */
    ESP_ERROR_CHECK(rgb_led_init());
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(touch_init());

    touch_register_callback(touch_event_handler);

    rgb_led_enable(true);
    rgb_led_set_brightness(20);

    enter_state(STATE_IDLE);

    xTaskCreate(control_loop_task, "ctrl_loop", 4096, NULL, 10, NULL);

    /* Start CLI console */
    cli_config_t cli_cfg = {
        .heater = heater,
        .motor = whisk_motor,
        .temp_sensor = water_temp,
    };
    ESP_ERROR_CHECK(cli_init(&cli_cfg));

    ESP_LOGI(TAG, "Matcha Machine ready");
}
