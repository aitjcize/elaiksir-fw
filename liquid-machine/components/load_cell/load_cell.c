#include "load_cell.h"
#include "board_hal.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "load_cell";

#define TARE_SAMPLES    16
#define STABILITY_WINDOW 5
#define STABILITY_THRESHOLD 50  /* Raw ADC counts */

typedef struct {
    load_cell_cfg_t cfg;
    nau7802_t       adc;
    i2c_master_bus_handle_t bus;
    int32_t         tare_offset;
    int32_t         scale_factor;   /* Raw counts per gram (×100 for precision) */
    int32_t         history[STABILITY_WINDOW];
    int             hist_idx;
    bool            initialized;
} load_cell_ctx_t;

static esp_err_t select_mux(load_cell_ctx_t *ctx)
{
    return tca9548_select_channel(ctx->cfg.mux, ctx->cfg.mux_channel);
}

static esp_err_t lc_init(weight_sensor_t *sensor)
{
    load_cell_ctx_t *ctx = (load_cell_ctx_t *)sensor->ctx;

    esp_err_t ret = select_mux(ctx);
    if (ret != ESP_OK) return ret;

    ret = nau7802_init(&ctx->adc, ctx->bus, NAU7802_GAIN_128, NAU7802_SPS_80);
    if (ret != ESP_OK) return ret;

    ctx->tare_offset = 0;
    ctx->scale_factor = 100;    /* Default: 1 count = 1 gram. Needs calibration. */
    ctx->initialized = true;

    /* Auto-tare on startup */
    ESP_LOGI(TAG, "%s: performing initial tare...", sensor->name);
    weight_sensor_tare(sensor);

    return ESP_OK;
}

static int32_t lc_read_raw(weight_sensor_t *sensor)
{
    load_cell_ctx_t *ctx = (load_cell_ctx_t *)sensor->ctx;
    if (!ctx->initialized) return 0;

    if (select_mux(ctx) != ESP_OK) return 0;

    int32_t raw;
    if (nau7802_read_raw(&ctx->adc, &raw) != ESP_OK) return 0;

    /* Update stability history */
    ctx->history[ctx->hist_idx] = raw;
    ctx->hist_idx = (ctx->hist_idx + 1) % STABILITY_WINDOW;

    return raw;
}

static int lc_read_grams(weight_sensor_t *sensor)
{
    load_cell_ctx_t *ctx = (load_cell_ctx_t *)sensor->ctx;
    int32_t raw = lc_read_raw(sensor);
    int32_t adjusted = raw - ctx->tare_offset;

    /* grams = adjusted * 100 / scale_factor */
    if (ctx->scale_factor == 0) return 0;
    return (int)(adjusted * 100 / ctx->scale_factor);
}

static esp_err_t lc_tare(weight_sensor_t *sensor)
{
    load_cell_ctx_t *ctx = (load_cell_ctx_t *)sensor->ctx;

    int64_t sum = 0;
    for (int i = 0; i < TARE_SAMPLES; i++) {
        int32_t raw = lc_read_raw(sensor);
        sum += raw;
    }
    ctx->tare_offset = (int32_t)(sum / TARE_SAMPLES);
    ESP_LOGI(TAG, "%s: tare offset = %ld", sensor->name, (long)ctx->tare_offset);
    return ESP_OK;
}

static esp_err_t lc_calibrate(weight_sensor_t *sensor, int known_grams)
{
    load_cell_ctx_t *ctx = (load_cell_ctx_t *)sensor->ctx;
    if (known_grams <= 0) return ESP_ERR_INVALID_ARG;

    /* Read several samples with known weight */
    int64_t sum = 0;
    for (int i = 0; i < TARE_SAMPLES; i++) {
        int32_t raw = lc_read_raw(sensor);
        sum += raw;
    }
    int32_t avg = (int32_t)(sum / TARE_SAMPLES);
    int32_t adjusted = avg - ctx->tare_offset;

    /* scale_factor = adjusted_counts * 100 / known_grams */
    ctx->scale_factor = adjusted * 100 / known_grams;
    ESP_LOGI(TAG, "%s: calibrated scale_factor = %ld (at %d g)",
             sensor->name, (long)ctx->scale_factor, known_grams);
    return ESP_OK;
}

static bool lc_is_stable(weight_sensor_t *sensor)
{
    load_cell_ctx_t *ctx = (load_cell_ctx_t *)sensor->ctx;

    int32_t min_val = ctx->history[0], max_val = ctx->history[0];
    for (int i = 1; i < STABILITY_WINDOW; i++) {
        if (ctx->history[i] < min_val) min_val = ctx->history[i];
        if (ctx->history[i] > max_val) max_val = ctx->history[i];
    }
    return (max_val - min_val) < STABILITY_THRESHOLD;
}

static const weight_sensor_ops_t load_cell_ops = {
    .init       = lc_init,
    .read_raw   = lc_read_raw,
    .read_grams = lc_read_grams,
    .tare       = lc_tare,
    .calibrate  = lc_calibrate,
    .is_stable  = lc_is_stable,
};

weight_sensor_t *load_cell_create(const char *name,
                                   i2c_master_bus_handle_t bus,
                                   const load_cell_cfg_t *cfg)
{
    load_cell_ctx_t *ctx = calloc(1, sizeof(load_cell_ctx_t));
    if (!ctx) return NULL;
    memcpy(&ctx->cfg, cfg, sizeof(load_cell_cfg_t));
    ctx->bus = bus;

    weight_sensor_t *sensor = calloc(1, sizeof(weight_sensor_t));
    if (!sensor) { free(ctx); return NULL; }

    sensor->name = name;
    sensor->ops  = &load_cell_ops;
    sensor->ctx  = ctx;
    return sensor;
}
