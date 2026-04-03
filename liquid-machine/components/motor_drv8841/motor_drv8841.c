#include "motor_drv8841.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "motor_drv8841";

/* Per-instance context stored in motor_t.ctx */
typedef struct {
    motor_drv8841_cfg_t cfg;
    bool                running;
    int                 current_speed;  /* -100 to +100 */
} motor_drv8841_ctx_t;

static esp_err_t select_pwm_mux(motor_drv8841_ctx_t *ctx)
{
    if (ctx->cfg.pwm_mux) {
        return tca9548_select_channel(ctx->cfg.pwm_mux, ctx->cfg.pwm_mux_channel);
    }
    return ESP_OK;
}

static esp_err_t select_adc_mux(motor_drv8841_ctx_t *ctx)
{
    if (ctx->cfg.adc_mux) {
        return tca9548_select_channel(ctx->cfg.adc_mux, ctx->cfg.adc_mux_channel);
    }
    return ESP_OK;
}

static esp_err_t drv8841_init(motor_t *motor)
{
    motor_drv8841_ctx_t *ctx = (motor_drv8841_ctx_t *)motor->ctx;

    /* Set both channels off (brake) */
    esp_err_t ret = select_pwm_mux(ctx);
    if (ret != ESP_OK) return ret;

    pca9685_set_full_off(ctx->cfg.pwm, ctx->cfg.pwm_ch_in1);
    pca9685_set_full_off(ctx->cfg.pwm, ctx->cfg.pwm_ch_in2);

    ctx->running = false;
    ctx->current_speed = 0;
    return ESP_OK;
}

static void drv8841_set_speed(motor_t *motor, int percent)
{
    motor_drv8841_ctx_t *ctx = (motor_drv8841_ctx_t *)motor->ctx;

    if (percent < -100) percent = -100;
    if (percent > 100) percent = 100;

    if (select_pwm_mux(ctx) != ESP_OK) return;

    if (percent == 0) {
        /* Brake: both LOW */
        pca9685_set_full_off(ctx->cfg.pwm, ctx->cfg.pwm_ch_in1);
        pca9685_set_full_off(ctx->cfg.pwm, ctx->cfg.pwm_ch_in2);
        ctx->running = false;
    } else if (percent > 0) {
        /* Forward: IN1=PWM, IN2=OFF */
        uint16_t duty = (uint16_t)((uint32_t)4095 * percent / 100);
        pca9685_set_duty(ctx->cfg.pwm, ctx->cfg.pwm_ch_in1, duty);
        pca9685_set_full_off(ctx->cfg.pwm, ctx->cfg.pwm_ch_in2);
        ctx->running = true;
    } else {
        /* Reverse: IN1=OFF, IN2=PWM */
        uint16_t duty = (uint16_t)((uint32_t)4095 * (-percent) / 100);
        pca9685_set_full_off(ctx->cfg.pwm, ctx->cfg.pwm_ch_in1);
        pca9685_set_duty(ctx->cfg.pwm, ctx->cfg.pwm_ch_in2, duty);
        ctx->running = true;
    }

    ctx->current_speed = percent;
    ESP_LOGD(TAG, "%s speed=%d%%", motor->name, percent);
}

static void drv8841_stop(motor_t *motor)
{
    drv8841_set_speed(motor, 0);
    ESP_LOGI(TAG, "%s stopped", motor->name);
}

static int drv8841_read_current_ma(motor_t *motor)
{
    motor_drv8841_ctx_t *ctx = (motor_drv8841_ctx_t *)motor->ctx;

    if (!ctx->cfg.adc) return 0;

    if (select_adc_mux(ctx) != ESP_OK) return 0;

    int16_t raw;
    if (ads1115_read_raw(ctx->cfg.adc, ctx->cfg.adc_channel, &raw) != ESP_OK) {
        return 0;
    }

    int mv = ads1115_raw_to_mv(ctx->cfg.adc, raw);
    /* I = V / R_shunt.  V is in mV, R is in mΩ → I in A.
     * Convert: I_mA = V_mV * 1000 / R_mOhm */
    if (ctx->cfg.shunt_mohm == 0) return 0;
    int current_ma = mv * 1000 / ctx->cfg.shunt_mohm;
    return current_ma > 0 ? current_ma : -current_ma;
}

static bool drv8841_is_running(motor_t *motor)
{
    motor_drv8841_ctx_t *ctx = (motor_drv8841_ctx_t *)motor->ctx;
    return ctx->running;
}

static const motor_ops_t drv8841_motor_ops = {
    .init           = drv8841_init,
    .set_speed      = drv8841_set_speed,
    .stop           = drv8841_stop,
    .read_current_ma = drv8841_read_current_ma,
    .is_running     = drv8841_is_running,
};

motor_t *motor_drv8841_create(const char *name, const motor_drv8841_cfg_t *cfg)
{
    motor_drv8841_ctx_t *ctx = calloc(1, sizeof(motor_drv8841_ctx_t));
    if (!ctx) return NULL;
    memcpy(&ctx->cfg, cfg, sizeof(motor_drv8841_cfg_t));

    motor_t *motor = calloc(1, sizeof(motor_t));
    if (!motor) { free(ctx); return NULL; }

    motor->name = name;
    motor->ops  = &drv8841_motor_ops;
    motor->ctx  = ctx;
    return motor;
}
