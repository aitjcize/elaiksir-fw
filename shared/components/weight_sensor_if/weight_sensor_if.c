#include "weight_sensor_if.h"
#include "esp_log.h"

static const char *TAG = "weight_if";

esp_err_t weight_sensor_init(weight_sensor_t *s)
{
    if (!s || !s->ops || !s->ops->init) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Initializing weight sensor '%s'", s->name ? s->name : "?");
    return s->ops->init(s);
}

int32_t weight_sensor_read_raw(weight_sensor_t *s)
{
    if (s && s->ops && s->ops->read_raw) return s->ops->read_raw(s);
    return 0;
}

int weight_sensor_read_grams(weight_sensor_t *s)
{
    if (s && s->ops && s->ops->read_grams) return s->ops->read_grams(s);
    return 0;
}

esp_err_t weight_sensor_tare(weight_sensor_t *s)
{
    if (s && s->ops && s->ops->tare) return s->ops->tare(s);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t weight_sensor_calibrate(weight_sensor_t *s, int known_grams)
{
    if (s && s->ops && s->ops->calibrate) return s->ops->calibrate(s, known_grams);
    return ESP_ERR_NOT_SUPPORTED;
}

bool weight_sensor_is_stable(weight_sensor_t *s)
{
    if (s && s->ops && s->ops->is_stable) return s->ops->is_stable(s);
    return false;
}
