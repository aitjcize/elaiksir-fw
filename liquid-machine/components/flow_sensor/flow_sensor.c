#include "flow_sensor.h"
#include "board_hal.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "flow_sensor";

static pcnt_unit_handle_t pcnt_unit = NULL;
static int64_t last_update_us = 0;
static int last_count = 0;

esp_err_t flow_sensor_init(flow_sensor_t *fs, float pulses_per_ml)
{
    esp_err_t ret;

    fs->total_pulses = 0;
    fs->pulses_per_ml = pulses_per_ml;
    fs->flow_rate_ml_s = 0;

    /* Configure PCNT unit */
    pcnt_unit_config_t unit_cfg = {
        .high_limit = 10000,
        .low_limit = -1,
    };
    ret = pcnt_new_unit(&unit_cfg, &pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT unit create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure PCNT channel — count rising edges on FLOW_IN */
    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num = BOARD_HAL_FLOW_IN_PIN,
        .level_gpio_num = -1,
    };
    pcnt_channel_handle_t chan;
    ret = pcnt_unit_add_channel(pcnt_unit, &chan_cfg, &chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT channel add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Count up on rising edge, ignore falling edge */
    pcnt_channel_set_edge_action(chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                       PCNT_CHANNEL_EDGE_ACTION_HOLD);

    /* Enable and start */
    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);

    last_update_us = esp_timer_get_time();
    last_count = 0;

    ESP_LOGI(TAG, "Flow sensor initialized (%.1f pulses/mL)", pulses_per_ml);
    return ESP_OK;
}

void flow_sensor_reset(flow_sensor_t *fs)
{
    fs->total_pulses = 0;
    fs->flow_rate_ml_s = 0;
    if (pcnt_unit) {
        pcnt_unit_clear_count(pcnt_unit);
    }
    last_count = 0;
    last_update_us = esp_timer_get_time();
}

esp_err_t flow_sensor_update(flow_sensor_t *fs)
{
    if (!pcnt_unit) return ESP_ERR_INVALID_STATE;

    int count;
    esp_err_t ret = pcnt_unit_get_count(pcnt_unit, &count);
    if (ret != ESP_OK) return ret;

    int64_t now_us = esp_timer_get_time();
    int64_t dt_us = now_us - last_update_us;

    if (dt_us > 0) {
        int delta = count - last_count;
        fs->total_pulses += delta;

        /* Flow rate: pulses/sec → mL/s */
        float pulses_per_sec = (float)delta * 1000000.0f / (float)dt_us;
        fs->flow_rate_ml_s = pulses_per_sec / fs->pulses_per_ml;
    }

    last_count = count;
    last_update_us = now_us;

    /* Reset PCNT if getting close to limit to avoid overflow */
    if (count > 9000) {
        pcnt_unit_clear_count(pcnt_unit);
        last_count = 0;
    }

    return ESP_OK;
}

float flow_sensor_get_volume_ml(flow_sensor_t *fs)
{
    if (fs->pulses_per_ml <= 0) return 0;
    return (float)fs->total_pulses / fs->pulses_per_ml;
}

float flow_sensor_get_rate_ml_s(flow_sensor_t *fs)
{
    return fs->flow_rate_ml_s;
}
