#include "temp_sensor.h"
#include "board_hal.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "temp_ntc";

#define ADC_MAX         4095
#define NUM_SAMPLES     16

static esp_err_t ntc_init(temp_sensor_t *sensor)
{
    ESP_LOGI(TAG, "NTC sensor initialized (B=%d, R_ref=%d, R_series=%d)",
             BOARD_HAL_NTC_B_PARAM, BOARD_HAL_NTC_R_REF, BOARD_HAL_NTC_R_SERIES);
    return ESP_OK;
}

static int adc_read_averaged(void)
{
    int sum = 0;
    int count = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int raw;
        if (adc_oneshot_read(adc1_handle, BOARD_HAL_NTC_ADC_CHAN, &raw) == ESP_OK) {
            sum += raw;
            count++;
        }
    }
    return count > 0 ? sum / count : -1;
}

static int ntc_read_resistance(temp_sensor_t *sensor)
{
    int raw = adc_read_averaged();
    if (raw <= 0 || raw >= ADC_MAX) {
        return -1;
    }

    /* R_ntc = R_series * raw / (ADC_MAX - raw) */
    int r_ntc = BOARD_HAL_NTC_R_SERIES * raw / (ADC_MAX - raw);
    return r_ntc;
}

static int ntc_read_deci_c(temp_sensor_t *sensor)
{
    int r_ntc = ntc_read_resistance(sensor);
    if (r_ntc <= 0) {
        ESP_LOGW(TAG, "NTC read error (open/short?)");
        return -9999;
    }

    /* B-parameter equation: 1/T = 1/T_ref + (1/B) * ln(R/R_ref) */
    double t_ref_k = (double)(BOARD_HAL_NTC_T_REF + 273.15);
    double inv_t = (1.0 / t_ref_k) +
                   (1.0 / (double)BOARD_HAL_NTC_B_PARAM) *
                   log((double)r_ntc / (double)BOARD_HAL_NTC_R_REF);
    double t_k = 1.0 / inv_t;
    double t_c = t_k - 273.15;

    return (int)(t_c * 10.0);
}

static bool ntc_is_healthy(temp_sensor_t *sensor)
{
    int r = ntc_read_resistance(sensor);
    /* Healthy if resistance is in plausible range (100Ω–1MΩ) */
    return (r > 100 && r < 1000000);
}

static const temp_sensor_ops_t ntc_ops = {
    .init           = ntc_init,
    .read_deci_c    = ntc_read_deci_c,
    .read_resistance = ntc_read_resistance,
    .is_healthy     = ntc_is_healthy,
};

static temp_sensor_t ntc_sensor = {
    .name = "water_temp",
    .ops  = &ntc_ops,
    .ctx  = NULL,
};

temp_sensor_t *temp_sensor_ntc_create(void)
{
    return &ntc_sensor;
}
