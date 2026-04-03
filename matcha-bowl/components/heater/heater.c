#include "heater.h"
#include "board_hal.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "heater_matcha";

/*
 * AC mains half-cycle period in microseconds.
 * 60 Hz → 8333 µs,  50 Hz → 10000 µs.
 */
#define HEATER_HALF_CYCLE_US    8333    /* 60 Hz — change to 10000 for 50 Hz */
#define TRIAC_GATE_PULSE_US     50

static esp_timer_handle_t phase_timer = NULL;
static esp_timer_handle_t gate_off_timer = NULL;
static volatile int phase_delay_us = 0;
static volatile bool relay_on = false;
static volatile int current_power = 0;

/* ISR: fire TRIAC gate after phase delay */
static void IRAM_ATTR phase_timer_cb(void *arg)
{
    gpio_set_level(BOARD_HAL_HEAT2_PIN, 1);
    esp_timer_start_once(gate_off_timer, TRIAC_GATE_PULSE_US);
}

/* ISR: turn off TRIAC gate after pulse */
static void IRAM_ATTR gate_off_timer_cb(void *arg)
{
    gpio_set_level(BOARD_HAL_HEAT2_PIN, 0);
}

/* ISR: zero-crossing detected — schedule TRIAC fire */
static void IRAM_ATTR zero_cross_isr(void *arg)
{
    if (phase_delay_us > 0 && phase_delay_us < HEATER_HALF_CYCLE_US) {
        esp_timer_start_once(phase_timer, phase_delay_us);
    }
}

static void set_triac_phase(int percent)
{
    if (percent <= 0) {
        phase_delay_us = 0;
        gpio_set_level(BOARD_HAL_HEAT2_PIN, 0);
    } else if (percent >= 100) {
        phase_delay_us = 100;   /* Fire right after zero crossing */
    } else {
        phase_delay_us = HEATER_HALF_CYCLE_US * (100 - percent) / 100;
    }
}

static void set_relay(bool on)
{
    relay_on = on;
    gpio_set_level(BOARD_HAL_HEAT1_PIN, on ? 1 : 0);
}

static esp_err_t matcha_heater_init(heater_t *heater)
{
    esp_err_t ret;

    esp_timer_create_args_t phase_args = {
        .callback = phase_timer_cb,
        .dispatch_method = ESP_TIMER_ISR,
        .name = "triac_phase",
    };
    ret = esp_timer_create(&phase_args, &phase_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create phase timer: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_timer_create_args_t gate_args = {
        .callback = gate_off_timer_cb,
        .dispatch_method = ESP_TIMER_ISR,
        .name = "triac_gate_off",
    };
    ret = esp_timer_create(&gate_args, &gate_off_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create gate-off timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_intr_type(BOARD_HAL_ZERO_CROSS_PIN, GPIO_INTR_POSEDGE);
    ret = gpio_isr_handler_add(BOARD_HAL_ZERO_CROSS_PIN, zero_cross_isr, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add zero-cross ISR: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Heater initialized (half-cycle=%d us)", HEATER_HALF_CYCLE_US);
    return ESP_OK;
}

static void matcha_heater_set_power(heater_t *heater, int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    current_power = percent;

    if (percent == 0) {
        set_relay(false);
        set_triac_phase(0);
    } else if (percent <= 50) {
        /* Low power: TRIAC only, relay off */
        set_relay(false);
        set_triac_phase(percent * 2);   /* Map 1–50% → TRIAC 2–100% */
    } else {
        /* High power: relay on + TRIAC for fine control */
        set_relay(true);
        set_triac_phase((percent - 50) * 2);  /* Map 51–100% → TRIAC 2–100% */
    }
    ESP_LOGI(TAG, "Heater power=%d%% (relay=%s, phase_delay=%d us)",
             percent, relay_on ? "ON" : "OFF", phase_delay_us);
}

static void matcha_heater_shutdown(heater_t *heater)
{
    current_power = 0;
    set_triac_phase(0);
    set_relay(false);
    ESP_LOGW(TAG, "Heater SHUTDOWN");
}

static bool matcha_heater_is_active(heater_t *heater)
{
    return current_power > 0;
}

static const heater_ops_t matcha_heater_ops = {
    .init       = matcha_heater_init,
    .set_power  = matcha_heater_set_power,
    .shutdown   = matcha_heater_shutdown,
    .is_active  = matcha_heater_is_active,
};

static heater_t matcha_heater = {
    .name = "water_heater",
    .ops  = &matcha_heater_ops,
    .ctx  = NULL,
};

heater_t *heater_matcha_create(void)
{
    return &matcha_heater;
}
