#include "http_api.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "api_order";

extern const http_api_config_t *http_api_get_config(void);

/*
 * Order execution state — only one order at a time.
 */
typedef enum {
    ORDER_IDLE,
    ORDER_RUNNING,
    ORDER_DONE,
    ORDER_ERROR,
    ORDER_CANCELLED,
} order_state_t;

typedef struct {
    int     motor_idx;
    int     speed_percent;
    int     amount_ml;
    float   ml_per_second;
} order_step_t;

#define MAX_ORDER_STEPS 10

static volatile order_state_t order_state = ORDER_IDLE;
static order_step_t order_steps[MAX_ORDER_STEPS];
static int order_num_steps = 0;
static volatile float order_progress = 0;
static volatile int order_current_step = 0;
static volatile bool order_cancel_flag = false;

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

static void order_exec_task(void *arg)
{
    const http_api_config_t *cfg = http_api_get_config();

    order_state = ORDER_RUNNING;
    order_progress = 0;
    order_cancel_flag = false;

    ESP_LOGI(TAG, "Order execution started (%d steps)", order_num_steps);

    /*
     * Execute steps concurrently (like milk_machine PoC):
     * Start all pumps, then poll until all done.
     */
    float durations[MAX_ORDER_STEPS];
    float max_duration = 0;

    /* Start all pumps */
    for (int i = 0; i < order_num_steps; i++) {
        order_step_t *step = &order_steps[i];
        if (step->motor_idx < 0 || step->motor_idx >= cfg->num_motors) continue;

        durations[i] = (step->ml_per_second > 0)
                        ? (float)step->amount_ml / step->ml_per_second
                        : 5.0f;
        if (durations[i] > max_duration) max_duration = durations[i];

        ESP_LOGI(TAG, "  Step %d: motor %d (%s), %d ml, %.1f s",
                 i, step->motor_idx, cfg->motors[step->motor_idx]->name,
                 step->amount_ml, durations[i]);
        motor_set_speed(cfg->motors[step->motor_idx], step->speed_percent);
    }

    /* Poll until all done or cancelled */
    int64_t start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (1) {
        if (order_cancel_flag) {
            ESP_LOGW(TAG, "Order cancelled");
            for (int i = 0; i < order_num_steps; i++) {
                if (order_steps[i].motor_idx >= 0 && order_steps[i].motor_idx < cfg->num_motors) {
                    motor_stop(cfg->motors[order_steps[i].motor_idx]);
                }
            }
            order_state = ORDER_CANCELLED;
            goto done;
        }

        int64_t elapsed_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - start_ms;
        float elapsed_s = (float)elapsed_ms / 1000.0f;

        /* Stop individual pumps as their duration completes */
        bool any_running = false;
        for (int i = 0; i < order_num_steps; i++) {
            if (elapsed_s >= durations[i]) {
                if (motor_is_running(cfg->motors[order_steps[i].motor_idx])) {
                    motor_stop(cfg->motors[order_steps[i].motor_idx]);
                    ESP_LOGI(TAG, "  Step %d complete", i);
                }
            } else {
                any_running = true;
            }
        }

        /* Update progress */
        order_progress = (max_duration > 0) ? (elapsed_s / max_duration) : 1.0f;
        if (order_progress > 1.0f) order_progress = 1.0f;

        if (!any_running) {
            ESP_LOGI(TAG, "Order complete");
            order_state = ORDER_DONE;
            order_progress = 1.0f;
            goto done;
        }

        /* Timeout safety (max 60s) */
        if (elapsed_ms > 60000) {
            ESP_LOGE(TAG, "Order timeout!");
            for (int i = 0; i < order_num_steps; i++) {
                motor_stop(cfg->motors[order_steps[i].motor_idx]);
            }
            order_state = ORDER_ERROR;
            goto done;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

done:
    vTaskDelete(NULL);
}

/*
 * POST /api/order/start
 * Body: {"steps": [{"motor": 0, "speed_percent": 80, "amount_ml": 150, "ml_per_second": 48}]}
 *
 * Starts concurrent pump execution. Poll /api/order/status for progress.
 */
static esp_err_t order_start_handler(httpd_req_t *req)
{
    if (order_state == ORDER_RUNNING) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"order already running\"}");
        return ESP_OK;
    }

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

    cJSON *steps = cJSON_GetObjectItem(root, "steps");
    if (!cJSON_IsArray(steps)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"missing steps array\"}");
        return ESP_OK;
    }

    order_num_steps = 0;
    cJSON *step;
    cJSON_ArrayForEach(step, steps) {
        if (order_num_steps >= MAX_ORDER_STEPS) break;
        order_step_t *s = &order_steps[order_num_steps++];
        s->motor_idx = cJSON_GetObjectItem(step, "motor") ?
                       cJSON_GetObjectItem(step, "motor")->valueint : -1;
        s->speed_percent = cJSON_GetObjectItem(step, "speed_percent") ?
                           cJSON_GetObjectItem(step, "speed_percent")->valueint : 80;
        s->amount_ml = cJSON_GetObjectItem(step, "amount_ml") ?
                       cJSON_GetObjectItem(step, "amount_ml")->valueint : 100;
        s->ml_per_second = cJSON_GetObjectItem(step, "ml_per_second") ?
                           (float)cJSON_GetObjectItem(step, "ml_per_second")->valuedouble : 48.0f;
    }
    cJSON_Delete(root);

    /* Launch execution task */
    order_state = ORDER_RUNNING;
    order_progress = 0;
    order_current_step = 0;
    xTaskCreate(order_exec_task, "order_exec", 4096, NULL, 8, NULL);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/*
 * POST /api/order/cancel
 */
static esp_err_t order_cancel_handler(httpd_req_t *req)
{
    if (order_state != ORDER_RUNNING) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no active order\"}");
        return ESP_OK;
    }
    order_cancel_flag = true;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/*
 * GET /api/order/status
 * Returns: {"state": "idle|running|done|error|cancelled", "progress": 0.0-1.0, "steps": N}
 */
static esp_err_t order_status_handler(httpd_req_t *req)
{
    const char *state_str;
    switch (order_state) {
    case ORDER_IDLE:      state_str = "idle"; break;
    case ORDER_RUNNING:   state_str = "running"; break;
    case ORDER_DONE:      state_str = "done"; break;
    case ORDER_ERROR:     state_str = "error"; break;
    case ORDER_CANCELLED: state_str = "cancelled"; break;
    default:              state_str = "unknown"; break;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", state_str);
    cJSON_AddNumberToObject(root, "progress", order_progress);
    cJSON_AddNumberToObject(root, "total_steps", order_num_steps);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    cJSON_Delete(root);

    /* Auto-reset to idle after client reads done/error/cancelled */
    if (order_state == ORDER_DONE || order_state == ORDER_ERROR ||
        order_state == ORDER_CANCELLED) {
        order_state = ORDER_IDLE;
    }

    return ESP_OK;
}

esp_err_t api_order_register(httpd_handle_t server)
{
    httpd_uri_t start_uri = {
        .uri = "/api/order/start", .method = HTTP_POST,
        .handler = order_start_handler,
    };
    httpd_register_uri_handler(server, &start_uri);

    httpd_uri_t cancel_uri = {
        .uri = "/api/order/cancel", .method = HTTP_POST,
        .handler = order_cancel_handler,
    };
    httpd_register_uri_handler(server, &cancel_uri);

    httpd_uri_t status_uri = {
        .uri = "/api/order/status", .method = HTTP_GET,
        .handler = order_status_handler,
    };
    httpd_register_uri_handler(server, &status_uri);

    ESP_LOGI(TAG, "Order API endpoints registered");
    return ESP_OK;
}
