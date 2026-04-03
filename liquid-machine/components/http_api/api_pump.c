#include "http_api.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "api_pump";

extern const http_api_config_t *http_api_get_config(void);

/* Helper: read request body into allocated buffer */
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

static void send_ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
}

static void send_error(httpd_req_t *req, int code, const char *msg)
{
    httpd_resp_set_status(req, code == 400 ? "400 Bad Request" : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", msg);
    httpd_resp_sendstr(req, buf);
}

/*
 * POST /api/pump/run
 * Body: {"pipes": [{"pipe": 0, "speed_percent": 70, "duration_ms": 3000}, ...]}
 *
 * Runs motors directly by index. For engineering/maintenance use.
 */
static esp_err_t pump_run_handler(httpd_req_t *req)
{
    const http_api_config_t *cfg = http_api_get_config();
    char *body = read_body(req);
    if (!body) { send_error(req, 400, "empty body"); return ESP_OK; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) { send_error(req, 400, "invalid json"); return ESP_OK; }

    cJSON *pipes = cJSON_GetObjectItem(root, "pipes");
    if (!cJSON_IsArray(pipes)) {
        cJSON_Delete(root);
        send_error(req, 400, "missing pipes array");
        return ESP_OK;
    }

    cJSON *pipe;
    cJSON_ArrayForEach(pipe, pipes) {
        int idx = cJSON_GetObjectItem(pipe, "pipe") ?
                  cJSON_GetObjectItem(pipe, "pipe")->valueint : -1;
        int speed = cJSON_GetObjectItem(pipe, "speed_percent") ?
                    cJSON_GetObjectItem(pipe, "speed_percent")->valueint : 50;

        if (idx < 0 || idx >= cfg->num_motors) {
            ESP_LOGW(TAG, "Invalid pipe index: %d", idx);
            continue;
        }

        ESP_LOGI(TAG, "Running pump %d (%s) at %d%%", idx, cfg->motors[idx]->name, speed);
        motor_set_speed(cfg->motors[idx], speed);

        /* If duration specified, schedule stop via a one-shot timer */
        int duration_ms = cJSON_GetObjectItem(pipe, "duration_ms") ?
                          cJSON_GetObjectItem(pipe, "duration_ms")->valueint : 0;
        if (duration_ms > 0) {
            /* Simple blocking approach for now — runs in HTTP handler context.
             * For production, use esp_timer for async stop. */
            vTaskDelay(pdMS_TO_TICKS(duration_ms));
            motor_stop(cfg->motors[idx]);
            ESP_LOGI(TAG, "Pump %d stopped after %d ms", idx, duration_ms);
        }
    }

    cJSON_Delete(root);
    send_ok(req);
    return ESP_OK;
}

/*
 * POST /api/pump/stop-all
 * Emergency stop all motors.
 */
static esp_err_t pump_stop_all_handler(httpd_req_t *req)
{
    const http_api_config_t *cfg = http_api_get_config();
    ESP_LOGW(TAG, "STOP ALL PUMPS");
    for (int i = 0; i < cfg->num_motors; i++) {
        motor_stop(cfg->motors[i]);
    }
    send_ok(req);
    return ESP_OK;
}

/*
 * GET /api/pump/status
 * Returns running state and current of each pump.
 */
static esp_err_t pump_status_handler(httpd_req_t *req)
{
    const http_api_config_t *cfg = http_api_get_config();
    cJSON *root = cJSON_CreateArray();

    for (int i = 0; i < cfg->num_motors; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "pipe", i);
        cJSON_AddStringToObject(item, "name", cfg->motors[i]->name);
        cJSON_AddBoolToObject(item, "running", motor_is_running(cfg->motors[i]));
        cJSON_AddNumberToObject(item, "current_ma", motor_read_current_ma(cfg->motors[i]));
        cJSON_AddItemToArray(root, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_pump_register(httpd_handle_t server)
{
    httpd_uri_t run_uri = {
        .uri = "/api/pump/run", .method = HTTP_POST,
        .handler = pump_run_handler,
    };
    httpd_register_uri_handler(server, &run_uri);

    httpd_uri_t stop_uri = {
        .uri = "/api/pump/stop-all", .method = HTTP_POST,
        .handler = pump_stop_all_handler,
    };
    httpd_register_uri_handler(server, &stop_uri);

    httpd_uri_t status_uri = {
        .uri = "/api/pump/status", .method = HTTP_GET,
        .handler = pump_status_handler,
    };
    httpd_register_uri_handler(server, &status_uri);

    ESP_LOGI(TAG, "Pump API endpoints registered");
    return ESP_OK;
}
