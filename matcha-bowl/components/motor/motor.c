#include "motor.h"
#include "board_hal.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "motor_matcha";

#define MOTOR_LEDC_TIMER        LEDC_TIMER_0
#define MOTOR_LEDC_CHANNEL      LEDC_CHANNEL_0
#define MOTOR_LEDC_SPEED_MODE   LEDC_LOW_SPEED_MODE
#define MOTOR_PWM_FREQ_HZ       20000   /* 20kHz — inaudible */
#define MOTOR_PWM_RESOLUTION    LEDC_TIMER_10_BIT
#define MOTOR_PWM_MAX_DUTY      ((1 << 10) - 1)

static bool running = false;

static void matcha_motor_stop(motor_t *motor);

static esp_err_t matcha_motor_init(motor_t *motor)
{
    esp_err_t ret;

    ledc_timer_config_t timer_conf = {
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .timer_num = MOTOR_LEDC_TIMER,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t chan_conf = {
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .channel = MOTOR_LEDC_CHANNEL,
        .timer_sel = MOTOR_LEDC_TIMER,
        .gpio_num = BOARD_HAL_MOTOR_PIN,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&chan_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Motor initialized (PWM %d Hz, 10-bit)", MOTOR_PWM_FREQ_HZ);
    return ESP_OK;
}

static void matcha_motor_set_speed(motor_t *motor, int percent)
{
    /* Forward-only: clamp to 0–100 */
    if (percent <= 0) {
        matcha_motor_stop(motor);
        return;
    }
    if (percent > 100) percent = 100;

    uint32_t duty = (uint32_t)MOTOR_PWM_MAX_DUTY * percent / 100;
    ledc_set_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_LEDC_CHANNEL, duty);
    ledc_update_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_LEDC_CHANNEL);
    running = true;
    ESP_LOGI(TAG, "Motor speed=%d%% (duty=%lu)", percent, (unsigned long)duty);
}

static void matcha_motor_stop(motor_t *motor)
{
    ledc_set_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_LEDC_CHANNEL);
    running = false;
    ESP_LOGI(TAG, "Motor stopped");
}

static int matcha_motor_read_current_ma(motor_t *motor)
{
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, BOARD_HAL_MOTOR_CS_ADC_CHAN, &raw);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return 0;
    }

    int voltage_mv = raw * 2200 / 4095;
    int current_ma = voltage_mv * 1000 / BOARD_HAL_MOTOR_SHUNT_MOHM;
    return current_ma;
}

static bool matcha_motor_is_running(motor_t *motor)
{
    return running;
}

static const motor_ops_t matcha_motor_ops = {
    .init           = matcha_motor_init,
    .set_speed      = matcha_motor_set_speed,
    .stop           = matcha_motor_stop,
    .read_current_ma = matcha_motor_read_current_ma,
    .is_running     = matcha_motor_is_running,
};

static motor_t matcha_motor = {
    .name = "whisk",
    .ops  = &matcha_motor_ops,
    .ctx  = NULL,
};

motor_t *motor_matcha_create(void)
{
    return &matcha_motor;
}
