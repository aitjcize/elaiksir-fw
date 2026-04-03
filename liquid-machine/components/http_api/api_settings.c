#include "http_api.h"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "api_settings";

#define NVS_NAMESPACE "settings"

extern const http_api_config_t *http_api_get_config(void);

static char *read_body(httpd_req_t *req)
{
    int len = req->content_len;
    if (len <= 0 || len > 4096) return NULL;
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    int ret = httpd_req_recv(req, buf, len);
    if (ret <= 0) { free(buf); return NULL; }
    buf[ret] = '\0';
    return buf;
}

/*
 * GET /api/settings
 * Returns current runtime settings from NVS.
 */
static esp_err_t settings_get_handler(httpd_req_t *req)
{
    const http_api_config_t *cfg = http_api_get_config();
    cJSON *root = cJSON_CreateObject();

    /* Pump config */
    cJSON *pumps = cJSON_CreateArray();
    for (int i = 0; i < cfg->num_motors; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "pipe", i);
        cJSON_AddStringToObject(p, "name", cfg->motors[i]->name);
        cJSON_AddItemToArray(pumps, p);
    }
    cJSON_AddItemToObject(root, "pumps", pumps);

    /* Reservoir full weights */
    cJSON *reservoirs = cJSON_CreateArray();
    for (int i = 0; i < cfg->num_scales; i++) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "id", cfg->scales[i]->name);
        int full = cfg->reservoir_full_weights ? cfg->reservoir_full_weights[i] : 5000;
        cJSON_AddNumberToObject(r, "full_weight_g", full);
        cJSON_AddItemToArray(reservoirs, r);
    }
    cJSON_AddItemToObject(root, "reservoirs", reservoirs);

    /* Temperature sensors */
    cJSON *temps = cJSON_CreateArray();
    for (int i = 0; i < cfg->num_temp_sensors; i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddStringToObject(t, "id", cfg->temp_sensors[i]->name);
        int temp = temp_sensor_read_deci_c(cfg->temp_sensors[i]);
        cJSON_AddNumberToObject(t, "temp_deci_c", temp);
        cJSON_AddItemToArray(temps, t);
    }
    cJSON_AddItemToObject(root, "temperatures", temps);

    /* Read any saved settings from NVS */
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        /* Example: read saved alert threshold */
        int32_t alert_ml = 500;
        nvs_get_i32(nvs, "alert_ml", &alert_ml);
        cJSON_AddNumberToObject(root, "alert_threshold_ml", alert_ml);
        nvs_close(nvs);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/*
 * POST /api/settings
 * Body: {"alert_threshold_ml": 300, "reservoir_full_weights": {"scale1": 5000, ...}}
 * Persists settings to NVS.
 */
static esp_err_t settings_post_handler(httpd_req_t *req)
{
    char *body = read_body(req);
    if (!body) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"empty body\"}");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid json\"}");
        return ESP_OK;
    }

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"nvs open failed\"}");
        return ESP_OK;
    }

    /* Save alert threshold */
    cJSON *alert = cJSON_GetObjectItem(root, "alert_threshold_ml");
    if (cJSON_IsNumber(alert)) {
        nvs_set_i32(nvs, "alert_ml", alert->valueint);
        ESP_LOGI(TAG, "Saved alert_threshold_ml = %d", alert->valueint);
    }

    /* Save reservoir full weights */
    http_api_config_t *cfg_mut = (http_api_config_t *)http_api_get_config();
    cJSON *weights = cJSON_GetObjectItem(root, "reservoir_full_weights");
    if (cJSON_IsObject(weights)) {
        for (int i = 0; i < cfg_mut->num_scales; i++) {
            cJSON *w = cJSON_GetObjectItem(weights, cfg_mut->scales[i]->name);
            if (cJSON_IsNumber(w) && cfg_mut->reservoir_full_weights) {
                cfg_mut->reservoir_full_weights[i] = w->valueint;
                char key[32];
                snprintf(key, sizeof(key), "full_w_%d", i);
                nvs_set_i32(nvs, key, w->valueint);
                ESP_LOGI(TAG, "Saved %s full_weight = %d g",
                         cfg_mut->scales[i]->name, w->valueint);
            }
        }
    }

    nvs_commit(nvs);
    nvs_close(nvs);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/*
 * POST /api/scale/tare
 * Body: {"id": "scale1"} or {"index": 0}
 */
static esp_err_t scale_tare_handler(httpd_req_t *req)
{
    const http_api_config_t *cfg = http_api_get_config();
    char *body = read_body(req);
    if (!body) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"empty body\"}");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid json\"}");
        return ESP_OK;
    }

    int idx = -1;
    cJSON *index_j = cJSON_GetObjectItem(root, "index");
    cJSON *id_j = cJSON_GetObjectItem(root, "id");

    if (cJSON_IsNumber(index_j)) {
        idx = index_j->valueint;
    } else if (cJSON_IsString(id_j)) {
        for (int i = 0; i < cfg->num_scales; i++) {
            if (strcmp(cfg->scales[i]->name, id_j->valuestring) == 0) {
                idx = i;
                break;
            }
        }
    }
    cJSON_Delete(root);

    if (idx < 0 || idx >= cfg->num_scales) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid scale\"}");
        return ESP_OK;
    }

    weight_sensor_tare(cfg->scales[idx]);
    ESP_LOGI(TAG, "Tared scale %d (%s)", idx, cfg->scales[idx]->name);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

esp_err_t api_settings_register(httpd_handle_t server)
{
    httpd_uri_t get_uri = {
        .uri = "/api/settings", .method = HTTP_GET,
        .handler = settings_get_handler,
    };
    httpd_register_uri_handler(server, &get_uri);

    httpd_uri_t post_uri = {
        .uri = "/api/settings", .method = HTTP_POST,
        .handler = settings_post_handler,
    };
    httpd_register_uri_handler(server, &post_uri);

    httpd_uri_t tare_uri = {
        .uri = "/api/scale/tare", .method = HTTP_POST,
        .handler = scale_tare_handler,
    };
    httpd_register_uri_handler(server, &tare_uri);

    ESP_LOGI(TAG, "Settings API endpoints registered");
    return ESP_OK;
}
