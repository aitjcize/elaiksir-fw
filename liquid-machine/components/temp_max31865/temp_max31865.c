#include "temp_max31865.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "temp_max31865";

typedef struct {
    max31865_t          rtd;
    spi_host_device_t   host;
    gpio_num_t          cs_pin;
    float               r_ref;
    float               r_nominal;
} temp_max31865_ctx_t;

static esp_err_t max_init(temp_sensor_t *sensor)
{
    temp_max31865_ctx_t *ctx = (temp_max31865_ctx_t *)sensor->ctx;
    return max31865_init(&ctx->rtd, ctx->host, ctx->cs_pin,
                         ctx->r_ref, ctx->r_nominal, MAX31865_WIRE_3);
}

static int max_read_deci_c(temp_sensor_t *sensor)
{
    temp_max31865_ctx_t *ctx = (temp_max31865_ctx_t *)sensor->ctx;
    return max31865_read_deci_c(&ctx->rtd);
}

static int max_read_resistance(temp_sensor_t *sensor)
{
    temp_max31865_ctx_t *ctx = (temp_max31865_ctx_t *)sensor->ctx;
    float r;
    if (max31865_read_resistance(&ctx->rtd, &r) != ESP_OK) return -1;
    return (int)(r * 10.0f);    /* Return in deci-ohms for integer precision */
}

static bool max_is_healthy(temp_sensor_t *sensor)
{
    temp_max31865_ctx_t *ctx = (temp_max31865_ctx_t *)sensor->ctx;
    uint8_t fault;
    if (max31865_read_fault(&ctx->rtd, &fault) != ESP_OK) return false;
    return fault == 0;
}

static const temp_sensor_ops_t max31865_sensor_ops = {
    .init           = max_init,
    .read_deci_c    = max_read_deci_c,
    .read_resistance = max_read_resistance,
    .is_healthy     = max_is_healthy,
};

temp_sensor_t *temp_sensor_max31865_create(const char *name,
                                            spi_host_device_t host,
                                            gpio_num_t cs_pin,
                                            float r_ref, float r_nominal)
{
    temp_max31865_ctx_t *ctx = calloc(1, sizeof(temp_max31865_ctx_t));
    if (!ctx) return NULL;
    ctx->host = host;
    ctx->cs_pin = cs_pin;
    ctx->r_ref = r_ref;
    ctx->r_nominal = r_nominal;

    temp_sensor_t *sensor = calloc(1, sizeof(temp_sensor_t));
    if (!sensor) { free(ctx); return NULL; }

    sensor->name = name;
    sensor->ops  = &max31865_sensor_ops;
    sensor->ctx  = ctx;
    return sensor;
}
