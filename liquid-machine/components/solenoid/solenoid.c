#include "solenoid.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "solenoid";

static esp_err_t select_mux(solenoid_t *sol)
{
    if (sol->cfg.mux) {
        return tca9548_select_channel(sol->cfg.mux, sol->cfg.mux_channel);
    }
    return ESP_OK;
}

esp_err_t solenoid_init(solenoid_t *sol, const solenoid_cfg_t *cfg)
{
    memcpy(&sol->cfg, cfg, sizeof(solenoid_cfg_t));
    sol->is_open = false;

    /* Ensure valve starts closed */
    return solenoid_close(sol);
}

esp_err_t solenoid_open(solenoid_t *sol)
{
    esp_err_t ret = select_mux(sol);
    if (ret != ESP_OK) return ret;

    /* Energize open direction: open=FULL, close=OFF */
    pca9685_set_full_on(sol->cfg.pwm, sol->cfg.pwm_ch_open);
    pca9685_set_full_off(sol->cfg.pwm, sol->cfg.pwm_ch_close);
    sol->is_open = true;

    ESP_LOGD(TAG, "Valve opened (ch %d)", sol->cfg.pwm_ch_open);
    return ESP_OK;
}

esp_err_t solenoid_close(solenoid_t *sol)
{
    esp_err_t ret = select_mux(sol);
    if (ret != ESP_OK) return ret;

    /* De-energize: both OFF (spring return closes valve) */
    pca9685_set_full_off(sol->cfg.pwm, sol->cfg.pwm_ch_open);
    pca9685_set_full_off(sol->cfg.pwm, sol->cfg.pwm_ch_close);
    sol->is_open = false;

    ESP_LOGD(TAG, "Valve closed (ch %d)", sol->cfg.pwm_ch_open);
    return ESP_OK;
}

bool solenoid_is_open(solenoid_t *sol)
{
    return sol->is_open;
}
