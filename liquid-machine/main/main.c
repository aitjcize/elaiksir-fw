#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"

#include "board_hal.h"
#include "motor_if.h"
#include "temp_sensor_if.h"
#include "weight_sensor_if.h"
#include "motor_drv8841.h"
#include "temp_max31865.h"
#include "load_cell.h"
#include "flow_sensor.h"
#include "solenoid.h"
#include "ethernet.h"
#include "tca9548.h"
#include "pca9685.h"
#include "ads1115.h"
#include "tca9555.h"
#include "http_api.h"
#include "cli.h"

static const char *TAG = "liquid";

/* ==================== Hardware instances ==================== */

/* I2C muxes */
static tca9548_t  i2c1_mux;
static tca9548_t  i2c2_mux;

/* PCA9685 PWM controllers */
static pca9685_t  pwm_motor_a;     /* Motors M1–M6 (I2C2 mux CH0) */
static pca9685_t  pwm_motor_b;     /* Motors M7–M10 (I2C2 mux CH5) */
static pca9685_t  pwm_solenoid;    /* Solenoid valves (I2C2 mux CH6) — adjust per actual wiring */
static pca9685_t  pwm_led;         /* RGB LEDs (I2C1 mux CH7) */

/* ADS1115 motor current ADCs */
static ads1115_t  adc_motor_0;     /* M1–M4 current (I2C2 mux CH2) */
static ads1115_t  adc_motor_1;     /* M5–M8 current (I2C2 mux CH3) */

/* TCA9555 GPIO expander */
static tca9555_t  gpio_exp;

/* Flow sensor */
static flow_sensor_t flow;

/* Peripheral arrays */
static motor_t         *motors[BOARD_HAL_NUM_MOTORS];
static temp_sensor_t   *temp_sensors[BOARD_HAL_NUM_TEMP_SENSORS];
static weight_sensor_t *scales[BOARD_HAL_NUM_LOAD_CELLS];
static solenoid_t       valves[BOARD_HAL_NUM_SOLENOIDS];
static int              reservoir_full_weights[BOARD_HAL_NUM_LOAD_CELLS];

/* ==================== Init and Main ==================== */

static esp_err_t init_i2c_peripherals(void)
{
    esp_err_t ret;

    ret = tca9548_init(&i2c1_mux, i2c1_bus, BOARD_HAL_I2C_MUX_ADDR);
    if (ret != ESP_OK) return ret;
    ret = tca9548_init(&i2c2_mux, i2c2_bus, BOARD_HAL_I2C_MUX_ADDR);
    if (ret != ESP_OK) return ret;

    /* Motor PWM group A (I2C2 mux CH0) */
    tca9548_select_channel(&i2c2_mux, 0);
    ret = pca9685_init(&pwm_motor_a, i2c2_bus, BOARD_HAL_PCA9685_ADDR, 1000);
    if (ret != ESP_OK) return ret;

    /* Motor PWM group B (I2C2 mux CH5) */
    tca9548_select_channel(&i2c2_mux, 5);
    ret = pca9685_init(&pwm_motor_b, i2c2_bus, BOARD_HAL_PCA9685_ADDR, 1000);
    if (ret != ESP_OK) return ret;

    /* Solenoid PWM (I2C2 mux CH6 — adjust per actual wiring) */
    tca9548_select_channel(&i2c2_mux, 6);
    ret = pca9685_init(&pwm_solenoid, i2c2_bus, BOARD_HAL_PCA9685_ADDR, 1000);
    if (ret != ESP_OK) return ret;

    /* Motor current ADCs */
    tca9548_select_channel(&i2c2_mux, 2);
    ret = ads1115_init(&adc_motor_0, i2c2_bus, BOARD_HAL_ADS1115_ADDR,
                       ADS1115_PGA_0512, ADS1115_SPS_128);
    if (ret != ESP_OK) return ret;

    tca9548_select_channel(&i2c2_mux, 3);
    ret = ads1115_init(&adc_motor_1, i2c2_bus, BOARD_HAL_ADS1115_ADDR,
                       ADS1115_PGA_0512, ADS1115_SPS_128);
    if (ret != ESP_OK) return ret;

    /* LED PWM (I2C1 mux CH7) */
    tca9548_select_channel(&i2c1_mux, 7);
    ret = pca9685_init(&pwm_led, i2c1_bus, BOARD_HAL_PCA9685_ADDR, 200);
    if (ret != ESP_OK) return ret;

    /* GPIO expander (direct I2C3) */
    ret = tca9555_init(&gpio_exp, i2c3_bus, BOARD_HAL_TCA9555_ADDR);
    if (ret != ESP_OK) return ret;

    tca9548_select_none(&i2c1_mux);
    tca9548_select_none(&i2c2_mux);

    ESP_LOGI(TAG, "I2C peripherals initialized");
    return ESP_OK;
}

static void create_motors(void)
{
    struct { const char *n; pca9685_t *p; uint8_t mch; uint8_t c1, c2;
             ads1115_t *a; uint8_t ach; ads1115_mux_t am; } m[] = {
        {"M1",  &pwm_motor_a, 0,  2,  3, &adc_motor_0, 2, ADS1115_MUX_AIN0_GND},
        {"M2",  &pwm_motor_a, 0,  0,  1, &adc_motor_0, 2, ADS1115_MUX_AIN1_GND},
        {"M3",  &pwm_motor_a, 0,  4,  5, &adc_motor_0, 2, ADS1115_MUX_AIN2_GND},
        {"M4",  &pwm_motor_a, 0,  6,  7, &adc_motor_0, 2, ADS1115_MUX_AIN3_GND},
        {"M5",  &pwm_motor_a, 0,  8,  9, &adc_motor_1, 3, ADS1115_MUX_AIN0_GND},
        {"M6",  &pwm_motor_a, 0, 10, 11, &adc_motor_1, 3, ADS1115_MUX_AIN1_GND},
        {"M7",  &pwm_motor_b, 5,  0,  1, &adc_motor_1, 3, ADS1115_MUX_AIN2_GND},
        {"M8",  &pwm_motor_b, 5,  2,  3, &adc_motor_1, 3, ADS1115_MUX_AIN3_GND},
        {"M9",  &pwm_motor_b, 5,  4,  5, NULL,         0, 0},
        {"M10", &pwm_motor_b, 5,  6,  7, NULL,         0, 0},
    };

    for (int i = 0; i < BOARD_HAL_NUM_MOTORS; i++) {
        motor_drv8841_cfg_t cfg = {
            .pwm_mux = &i2c2_mux, .pwm_mux_channel = m[i].mch,
            .pwm = m[i].p, .pwm_ch_in1 = m[i].c1, .pwm_ch_in2 = m[i].c2,
            .adc_mux = m[i].a ? &i2c2_mux : NULL, .adc_mux_channel = m[i].ach,
            .adc = m[i].a, .adc_channel = m[i].am, .shunt_mohm = 252,
        };
        motors[i] = motor_drv8841_create(m[i].n, &cfg);
        if (motors[i]) motor_init(motors[i]);
    }
    ESP_LOGI(TAG, "%d motors initialized", BOARD_HAL_NUM_MOTORS);
}

static void create_temp_sensors(void)
{
    temp_sensors[0] = temp_sensor_max31865_create(
        "temp1", SPI2_HOST, BOARD_HAL_TEMP1_CS_PIN, 4300.0f, 1000.0f);
    temp_sensors[1] = temp_sensor_max31865_create(
        "temp2", SPI2_HOST, BOARD_HAL_TEMP2_CS_PIN, 4300.0f, 1000.0f);
    for (int i = 0; i < BOARD_HAL_NUM_TEMP_SENSORS; i++) {
        if (temp_sensors[i]) temp_sensor_init(temp_sensors[i]);
    }
    ESP_LOGI(TAG, "%d temperature sensors initialized", BOARD_HAL_NUM_TEMP_SENSORS);
}

static void create_load_cells(void)
{
    const char *names[] = {"scale1", "scale2", "scale3", "scale4"};
    for (int i = 0; i < BOARD_HAL_NUM_LOAD_CELLS; i++) {
        load_cell_cfg_t cfg = {
            .mux = &i2c1_mux,
            .mux_channel = (uint8_t)i,  /* CH0–CH3 */
        };
        scales[i] = load_cell_create(names[i], i2c1_bus, &cfg);
        if (scales[i]) weight_sensor_init(scales[i]);
    }
    ESP_LOGI(TAG, "%d load cells initialized", BOARD_HAL_NUM_LOAD_CELLS);
}

static void create_solenoids(void)
{
    /* Solenoid valves on DRV8841_C (behind I2C2 mux CH6).
     * Each valve uses 2 PCA9685 channels (open/close direction). */
    for (int i = 0; i < BOARD_HAL_NUM_SOLENOIDS; i++) {
        solenoid_cfg_t cfg = {
            .mux = &i2c2_mux,
            .mux_channel = 6,
            .pwm = &pwm_solenoid,
            .pwm_ch_open = (uint8_t)(i * 2),
            .pwm_ch_close = (uint8_t)(i * 2 + 1),
        };
        solenoid_init(&valves[i], &cfg);
    }
    ESP_LOGI(TAG, "%d solenoid valves initialized", BOARD_HAL_NUM_SOLENOIDS);
}

static void control_loop_task(void *arg)
{
    while (1) {
        /* Update flow sensor */
        flow_sensor_update(&flow);

        /* Periodic telemetry logging */
        static int log_counter = 0;
        if (++log_counter >= 20) {  /* Every 2 seconds */
            log_counter = 0;

            for (int i = 0; i < BOARD_HAL_NUM_TEMP_SENSORS; i++) {
                if (temp_sensors[i]) {
                    int t = temp_sensor_read_deci_c(temp_sensors[i]);
                    if (t != -9999) {
                        ESP_LOGI(TAG, "%s: %d.%d C", temp_sensors[i]->name,
                                 t / 10, t % 10);
                    }
                }
            }

            if (ethernet_is_connected()) {
                ESP_LOGD(TAG, "IP: %s", ethernet_get_ip());
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Liquid Machine firmware starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Board hardware */
    ESP_ERROR_CHECK(board_hal_init());

    /* SPI bus for MAX31865 */
    spi_bus_config_t spi_cfg = {
        .mosi_io_num = BOARD_HAL_SPI_MOSI_PIN,
        .miso_io_num = BOARD_HAL_SPI_MISO_PIN,
        .sclk_io_num = BOARD_HAL_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO));

    /* I2C peripherals */
    ESP_ERROR_CHECK(init_i2c_peripherals());

    /* Create all peripheral instances */
    create_motors();
    create_temp_sensors();
    create_load_cells();
    create_solenoids();

    /* Flow sensor (calibration TBD — using 5.0 pulses/mL as placeholder) */
    ESP_ERROR_CHECK(flow_sensor_init(&flow, 5.0f));

    /* Ethernet */
    ESP_ERROR_CHECK(ethernet_init());

    /* Default reservoir full weights (grams) */
    for (int i = 0; i < BOARD_HAL_NUM_LOAD_CELLS; i++) {
        reservoir_full_weights[i] = 5000;
    }

    /* HTTP API server */
    http_api_config_t api_cfg = {
        .motors = motors,
        .num_motors = BOARD_HAL_NUM_MOTORS,
        .scales = scales,
        .num_scales = BOARD_HAL_NUM_LOAD_CELLS,
        .temp_sensors = temp_sensors,
        .num_temp_sensors = BOARD_HAL_NUM_TEMP_SENSORS,
        .reservoir_full_weights = reservoir_full_weights,
    };
    ESP_ERROR_CHECK(http_api_start(&api_cfg));

    /* Start control loop */
    xTaskCreate(control_loop_task, "ctrl_loop", 8192, NULL, 10, NULL);

    /* Start CLI console */
    cli_config_t cli_cfg = {
        .motors = motors,
        .num_motors = BOARD_HAL_NUM_MOTORS,
        .temp_sensors = temp_sensors,
        .num_temp_sensors = BOARD_HAL_NUM_TEMP_SENSORS,
        .scales = scales,
        .num_scales = BOARD_HAL_NUM_LOAD_CELLS,
        .valves = valves,
        .num_valves = BOARD_HAL_NUM_SOLENOIDS,
        .flow = &flow,
    };
    ESP_ERROR_CHECK(cli_init(&cli_cfg));

    ESP_LOGI(TAG, "Liquid Machine ready: %d motors, %d valves, %d scales, %d temp sensors",
             BOARD_HAL_NUM_MOTORS, BOARD_HAL_NUM_SOLENOIDS,
             BOARD_HAL_NUM_LOAD_CELLS, BOARD_HAL_NUM_TEMP_SENSORS);
}
