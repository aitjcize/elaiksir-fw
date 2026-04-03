#include "motor_if.h"
#include "esp_log.h"

static const char *TAG = "motor_if";

esp_err_t motor_init(motor_t *motor)
{
    if (!motor || !motor->ops || !motor->ops->init) {
        ESP_LOGE(TAG, "Invalid motor or missing init op");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Initializing motor '%s'", motor->name ? motor->name : "?");
    return motor->ops->init(motor);
}

void motor_set_speed(motor_t *motor, int percent)
{
    if (motor && motor->ops && motor->ops->set_speed) {
        motor->ops->set_speed(motor, percent);
    }
}

void motor_stop(motor_t *motor)
{
    if (motor && motor->ops && motor->ops->stop) {
        motor->ops->stop(motor);
    }
}

int motor_read_current_ma(motor_t *motor)
{
    if (motor && motor->ops && motor->ops->read_current_ma) {
        return motor->ops->read_current_ma(motor);
    }
    return 0;
}

bool motor_is_running(motor_t *motor)
{
    if (motor && motor->ops && motor->ops->is_running) {
        return motor->ops->is_running(motor);
    }
    return false;
}
