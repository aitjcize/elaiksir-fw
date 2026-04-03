#include "heater_if.h"
#include "esp_log.h"

static const char *TAG = "heater_if";

esp_err_t heater_init(heater_t *heater)
{
    if (!heater || !heater->ops || !heater->ops->init) {
        ESP_LOGE(TAG, "Invalid heater or missing init op");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Initializing heater '%s'", heater->name ? heater->name : "?");
    return heater->ops->init(heater);
}

void heater_set_power(heater_t *heater, int percent)
{
    if (heater && heater->ops && heater->ops->set_power) {
        heater->ops->set_power(heater, percent);
    }
}

void heater_shutdown(heater_t *heater)
{
    if (heater && heater->ops && heater->ops->shutdown) {
        heater->ops->shutdown(heater);
    }
}

bool heater_is_active(heater_t *heater)
{
    if (heater && heater->ops && heater->ops->is_active) {
        return heater->ops->is_active(heater);
    }
    return false;
}
