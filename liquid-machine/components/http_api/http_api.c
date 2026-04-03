#include "http_api.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "http_api";

static httpd_handle_t server = NULL;
static http_api_config_t cfg;

/* Accessor for other API modules */
const http_api_config_t *http_api_get_config(void)
{
    return &cfg;
}

/* --- GET /api/capabilities --- */
static esp_err_t capabilities_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateArray();

    for (int i = 0; i < cfg.num_motors; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", cfg.motors[i]->name);
        cJSON_AddStringToObject(item, "type", "pump");
        cJSON_AddBoolToObject(item, "running", motor_is_running(cfg.motors[i]));
        cJSON_AddItemToArray(root, item);
    }

    for (int i = 0; i < cfg.num_scales; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", cfg.scales[i]->name);
        cJSON_AddStringToObject(item, "type", "scale");
        cJSON_AddNumberToObject(item, "weight_g", weight_sensor_read_grams(cfg.scales[i]));
        cJSON_AddItemToArray(root, item);
    }

    for (int i = 0; i < cfg.num_temp_sensors; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", cfg.temp_sensors[i]->name);
        cJSON_AddStringToObject(item, "type", "temperature");
        int t = temp_sensor_read_deci_c(cfg.temp_sensors[i]);
        cJSON_AddNumberToObject(item, "temp_deci_c", t);
        cJSON_AddItemToArray(root, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* --- GET /api/reservoir/levels --- */
static esp_err_t reservoir_levels_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateArray();

    for (int i = 0; i < cfg.num_scales; i++) {
        int weight = weight_sensor_read_grams(cfg.scales[i]);
        int full = (cfg.reservoir_full_weights && i < cfg.num_scales)
                   ? cfg.reservoir_full_weights[i] : 5000;
        int percent = full > 0 ? (weight * 100 / full) : 0;
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", cfg.scales[i]->name);
        cJSON_AddNumberToObject(item, "weight_g", weight);
        cJSON_AddNumberToObject(item, "full_weight_g", full);
        cJSON_AddNumberToObject(item, "percent", percent);
        cJSON_AddBoolToObject(item, "low", percent < 10);
        cJSON_AddBoolToObject(item, "stable", weight_sensor_is_stable(cfg.scales[i]));
        cJSON_AddItemToArray(root, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t http_api_start(const http_api_config_t *config)
{
    memcpy(&cfg, config, sizeof(http_api_config_t));

    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.max_uri_handlers = 16;
    http_cfg.uri_match_fn = httpd_uri_match_wildcard;
    http_cfg.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server, &http_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register core endpoints */
    httpd_uri_t capabilities_uri = {
        .uri = "/api/capabilities", .method = HTTP_GET,
        .handler = capabilities_handler,
    };
    httpd_register_uri_handler(server, &capabilities_uri);

    httpd_uri_t reservoir_uri = {
        .uri = "/api/reservoir/levels", .method = HTTP_GET,
        .handler = reservoir_levels_handler,
    };
    httpd_register_uri_handler(server, &reservoir_uri);

    /* Register sub-modules */
    api_pump_register(server);
    api_order_register(server);
    api_settings_register(server);

    ESP_LOGI(TAG, "HTTP API server started on port %d", http_cfg.server_port);
    return ESP_OK;
}

esp_err_t http_api_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    return ESP_OK;
}
