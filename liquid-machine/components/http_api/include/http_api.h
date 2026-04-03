#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "motor_if.h"
#include "weight_sensor_if.h"
#include "temp_sensor_if.h"

/**
 * Liquid-machine HTTP API server.
 *
 * Endpoints (modeled after milk_machine PoC):
 *
 *   GET  /api/capabilities     — list all motors/valves/sensors
 *   GET  /api/reservoir/levels  — load cell weights + fill %
 *   POST /api/pump/run          — run pump(s) directly
 *   POST /api/pump/stop-all     — emergency stop
 *   POST /api/order/start       — start a dispense (SSE progress stream)
 *   POST /api/order/cancel      — cancel active dispense
 *   GET  /api/order/status      — poll current order state
 *   GET  /api/settings          — get runtime settings
 *   POST /api/settings          — update runtime settings
 */

typedef struct {
    motor_t         **motors;
    int               num_motors;
    weight_sensor_t **scales;
    int               num_scales;
    temp_sensor_t   **temp_sensors;
    int               num_temp_sensors;
    /* Full weights for percentage calculation (grams) */
    int              *reservoir_full_weights;
} http_api_config_t;

/**
 * Start HTTP server and register all API endpoints.
 */
esp_err_t http_api_start(const http_api_config_t *config);

/**
 * Stop HTTP server.
 */
esp_err_t http_api_stop(void);

/* --- Internal: endpoint registration (called by http_api_start) --- */
esp_err_t api_pump_register(httpd_handle_t server);
esp_err_t api_order_register(httpd_handle_t server);
esp_err_t api_settings_register(httpd_handle_t server);
