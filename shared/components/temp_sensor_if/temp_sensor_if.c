#include "temp_sensor_if.h"
#include "esp_log.h"

static const char *TAG = "temp_sensor_if";

esp_err_t temp_sensor_init(temp_sensor_t *sensor)
{
    if (!sensor || !sensor->ops || !sensor->ops->init) {
        ESP_LOGE(TAG, "Invalid sensor or missing init op");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Initializing sensor '%s'", sensor->name ? sensor->name : "?");
    return sensor->ops->init(sensor);
}

int temp_sensor_read_deci_c(temp_sensor_t *sensor)
{
    if (sensor && sensor->ops && sensor->ops->read_deci_c) {
        return sensor->ops->read_deci_c(sensor);
    }
    return -9999;
}

int temp_sensor_read_resistance(temp_sensor_t *sensor)
{
    if (sensor && sensor->ops && sensor->ops->read_resistance) {
        return sensor->ops->read_resistance(sensor);
    }
    return -1;
}

bool temp_sensor_is_healthy(temp_sensor_t *sensor)
{
    if (sensor && sensor->ops && sensor->ops->is_healthy) {
        return sensor->ops->is_healthy(sensor);
    }
    return false;
}
