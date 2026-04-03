#include "cli.h"
#include "esp_console.h"
#include "esp_log.h"
#include "board_hal.h"
#include "ethernet.h"
#include "tca9548.h"
#include "tca9555.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "cli";

static cli_config_t cfg;

/* ==================== pump ==================== */

static struct {
    struct arg_int *index;
    struct arg_int *speed;
    struct arg_end *end;
} pump_args;

static int cmd_pump(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&pump_args);
    if (nerrors != 0) { arg_print_errors(stderr, pump_args.end, argv[0]); return 1; }

    int idx = pump_args.index->ival[0];
    int speed = pump_args.speed->count > 0 ? pump_args.speed->ival[0] : 0;

    if (idx < 0 || idx >= cfg.num_motors) {
        printf("Invalid pump index (0-%d)\n", cfg.num_motors - 1);
        return 1;
    }

    if (speed == 0) {
        motor_stop(cfg.motors[idx]);
        printf("Pump %d (%s) stopped\n", idx, cfg.motors[idx]->name);
    } else {
        motor_set_speed(cfg.motors[idx], speed);
        printf("Pump %d (%s) running at %d%%\n", idx, cfg.motors[idx]->name, speed);
    }
    return 0;
}

/* ==================== stop-all ==================== */

static int cmd_stop_all(int argc, char **argv)
{
    for (int i = 0; i < cfg.num_motors; i++) {
        motor_stop(cfg.motors[i]);
    }
    for (int i = 0; i < cfg.num_valves; i++) {
        solenoid_close(&cfg.valves[i]);
    }
    printf("All pumps stopped, all valves closed\n");
    return 0;
}

/* ==================== valve ==================== */

static struct {
    struct arg_int *index;
    struct arg_str *action;
    struct arg_end *end;
} valve_args;

static int cmd_valve(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&valve_args);
    if (nerrors != 0) { arg_print_errors(stderr, valve_args.end, argv[0]); return 1; }

    int idx = valve_args.index->ival[0];
    const char *action = valve_args.action->sval[0];

    if (idx < 0 || idx >= cfg.num_valves) {
        printf("Invalid valve index (0-%d)\n", cfg.num_valves - 1);
        return 1;
    }

    if (strcmp(action, "open") == 0) {
        solenoid_open(&cfg.valves[idx]);
        printf("Valve %d opened\n", idx);
    } else if (strcmp(action, "close") == 0) {
        solenoid_close(&cfg.valves[idx]);
        printf("Valve %d closed\n", idx);
    } else {
        printf("Usage: valve <index> <open|close>\n");
    }
    return 0;
}

/* ==================== scale ==================== */

static struct {
    struct arg_int *index;
    struct arg_str *action;
    struct arg_end *end;
} scale_args;

static int cmd_scale(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&scale_args);
    if (nerrors != 0) { arg_print_errors(stderr, scale_args.end, argv[0]); return 1; }

    int idx = scale_args.index->count > 0 ? scale_args.index->ival[0] : -1;
    const char *action = scale_args.action->count > 0 ? scale_args.action->sval[0] : "read";

    if (strcmp(action, "tare") == 0 && idx >= 0 && idx < cfg.num_scales) {
        weight_sensor_tare(cfg.scales[idx]);
        printf("Scale %d (%s) tared\n", idx, cfg.scales[idx]->name);
        return 0;
    }

    /* Read all or specific */
    int start = (idx >= 0 && idx < cfg.num_scales) ? idx : 0;
    int end = (idx >= 0 && idx < cfg.num_scales) ? idx + 1 : cfg.num_scales;

    for (int i = start; i < end; i++) {
        int g = weight_sensor_read_grams(cfg.scales[i]);
        int32_t raw = weight_sensor_read_raw(cfg.scales[i]);
        printf("Scale %d (%s): %d g (raw: %ld, stable: %s)\n",
               i, cfg.scales[i]->name, g, (long)raw,
               weight_sensor_is_stable(cfg.scales[i]) ? "yes" : "no");
    }
    return 0;
}

/* ==================== temp ==================== */

static int cmd_temp(int argc, char **argv)
{
    for (int i = 0; i < cfg.num_temp_sensors; i++) {
        int t = temp_sensor_read_deci_c(cfg.temp_sensors[i]);
        if (t == -9999) {
            printf("%s: READ ERROR\n", cfg.temp_sensors[i]->name);
        } else {
            printf("%s: %d.%d C\n", cfg.temp_sensors[i]->name, t / 10, t % 10);
        }
    }
    return 0;
}

/* ==================== flow ==================== */

static int cmd_flow(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "reset") == 0) {
        flow_sensor_reset(cfg.flow);
        printf("Flow sensor reset\n");
        return 0;
    }

    flow_sensor_update(cfg.flow);
    printf("Flow rate: %.2f mL/s\n", flow_sensor_get_rate_ml_s(cfg.flow));
    printf("Total volume: %.1f mL\n", flow_sensor_get_volume_ml(cfg.flow));
    return 0;
}

/* ==================== net ==================== */

static int cmd_net(int argc, char **argv)
{
    printf("Ethernet: %s\n", ethernet_is_connected() ? "connected" : "disconnected");
    if (ethernet_is_connected()) {
        printf("IP: %s\n", ethernet_get_ip());
    }
    return 0;
}

/* ==================== status ==================== */

static int cmd_status(int argc, char **argv)
{
    printf("=== Liquid Machine Status ===\n\n");

    printf("--- Pumps ---\n");
    for (int i = 0; i < cfg.num_motors; i++) {
        printf("  %s: %s", cfg.motors[i]->name,
               motor_is_running(cfg.motors[i]) ? "RUNNING" : "off");
        if (motor_is_running(cfg.motors[i])) {
            printf(" (%d mA)", motor_read_current_ma(cfg.motors[i]));
        }
        printf("\n");
    }

    printf("\n--- Valves ---\n");
    for (int i = 0; i < cfg.num_valves; i++) {
        printf("  V%d: %s\n", i, solenoid_is_open(&cfg.valves[i]) ? "OPEN" : "closed");
    }

    printf("\n--- Scales ---\n");
    for (int i = 0; i < cfg.num_scales; i++) {
        printf("  %s: %d g\n", cfg.scales[i]->name,
               weight_sensor_read_grams(cfg.scales[i]));
    }

    printf("\n--- Temperature ---\n");
    for (int i = 0; i < cfg.num_temp_sensors; i++) {
        int t = temp_sensor_read_deci_c(cfg.temp_sensors[i]);
        printf("  %s: %s", cfg.temp_sensors[i]->name,
               t == -9999 ? "ERROR" : "");
        if (t != -9999) printf("%d.%d C", t / 10, t % 10);
        printf("\n");
    }

    printf("\n--- Flow ---\n");
    printf("  Rate: %.2f mL/s, Total: %.1f mL\n",
           flow_sensor_get_rate_ml_s(cfg.flow),
           flow_sensor_get_volume_ml(cfg.flow));

    printf("\n--- Network ---\n");
    printf("  Ethernet: %s", ethernet_is_connected() ? "connected" : "disconnected");
    if (ethernet_is_connected()) printf(" (%s)", ethernet_get_ip());
    printf("\n");

    return 0;
}

/* ==================== registration ==================== */

esp_err_t cli_init(const cli_config_t *config)
{
    cfg = *config;

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "liquid>";
    repl_config.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* pump <index> [speed] — 0 to stop */
    pump_args.index = arg_int1(NULL, NULL, "<index>", "Pump index (0-9)");
    pump_args.speed = arg_int0(NULL, NULL, "<speed>", "Speed % (0=stop, default 0)");
    pump_args.end = arg_end(2);
    esp_console_cmd_t pump_cmd = {
        .command = "pump", .help = "Control pump: pump <index> [speed%]",
        .func = cmd_pump, .argtable = &pump_args,
    };
    esp_console_cmd_register(&pump_cmd);

    /* stop-all */
    esp_console_cmd_t stop_cmd = {
        .command = "stop-all", .help = "Emergency stop all pumps and close all valves",
        .func = cmd_stop_all,
    };
    esp_console_cmd_register(&stop_cmd);

    /* valve <index> <open|close> */
    valve_args.index = arg_int1(NULL, NULL, "<index>", "Valve index");
    valve_args.action = arg_str1(NULL, NULL, "<action>", "open/close");
    valve_args.end = arg_end(2);
    esp_console_cmd_t valve_cmd = {
        .command = "valve", .help = "Control valve: valve <index> <open|close>",
        .func = cmd_valve, .argtable = &valve_args,
    };
    esp_console_cmd_register(&valve_cmd);

    /* scale [index] [tare] */
    scale_args.index = arg_int0(NULL, NULL, "<index>", "Scale index (omit for all)");
    scale_args.action = arg_str0(NULL, NULL, "<action>", "read/tare (default: read)");
    scale_args.end = arg_end(2);
    esp_console_cmd_t scale_cmd = {
        .command = "scale", .help = "Read scales or tare: scale [index] [tare]",
        .func = cmd_scale, .argtable = &scale_args,
    };
    esp_console_cmd_register(&scale_cmd);

    /* temp */
    esp_console_cmd_t temp_cmd = {
        .command = "temp", .help = "Read all temperature sensors",
        .func = cmd_temp,
    };
    esp_console_cmd_register(&temp_cmd);

    /* flow [reset] */
    esp_console_cmd_t flow_cmd = {
        .command = "flow", .help = "Read flow sensor or reset: flow [reset]",
        .func = cmd_flow,
    };
    esp_console_cmd_register(&flow_cmd);

    /* net */
    esp_console_cmd_t net_cmd = {
        .command = "net", .help = "Show Ethernet status",
        .func = cmd_net,
    };
    esp_console_cmd_register(&net_cmd);

    /* status */
    esp_console_cmd_t status_cmd = {
        .command = "status", .help = "Show all system status",
        .func = cmd_status,
    };
    esp_console_cmd_register(&status_cmd);

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "CLI console started");
    return ESP_OK;
}
