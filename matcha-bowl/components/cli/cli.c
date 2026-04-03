#include "cli.h"
#include "esp_console.h"
#include "esp_log.h"
#include "board_hal.h"
#include "heater.h"
#include "motor.h"
#include "temp_sensor.h"
#include "touch.h"
#include "rgb_led.h"
#include "display.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "cli";

static cli_config_t cfg;

/* ==================== heater ==================== */

static struct {
    struct arg_str *action;
    struct arg_int *power;
    struct arg_end *end;
} heater_args;

static int cmd_heater(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&heater_args);
    if (nerrors != 0) { arg_print_errors(stderr, heater_args.end, argv[0]); return 1; }

    const char *action = heater_args.action->sval[0];

    if (strcmp(action, "on") == 0) {
        int pwr = heater_args.power->count > 0 ? heater_args.power->ival[0] : 80;
        heater_set_power(cfg.heater, pwr);
        printf("Heater power set to %d%%\n", pwr);
    } else if (strcmp(action, "off") == 0) {
        heater_shutdown(cfg.heater);
        printf("Heater OFF\n");
    } else if (strcmp(action, "status") == 0) {
        printf("Heater active: %s\n", heater_is_active(cfg.heater) ? "yes" : "no");
    } else {
        printf("Usage: heater <on [power]|off|status>\n");
    }
    return 0;
}

/* ==================== motor ==================== */

static struct {
    struct arg_str *action;
    struct arg_int *speed;
    struct arg_end *end;
} motor_args;

static int cmd_motor(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&motor_args);
    if (nerrors != 0) { arg_print_errors(stderr, motor_args.end, argv[0]); return 1; }

    const char *action = motor_args.action->sval[0];

    if (strcmp(action, "on") == 0 || strcmp(action, "speed") == 0) {
        int spd = motor_args.speed->count > 0 ? motor_args.speed->ival[0] : 50;
        motor_set_speed(cfg.motor, spd);
        printf("Motor speed set to %d%%\n", spd);
    } else if (strcmp(action, "off") == 0 || strcmp(action, "stop") == 0) {
        motor_stop(cfg.motor);
        printf("Motor stopped\n");
    } else if (strcmp(action, "status") == 0) {
        printf("Motor running: %s\n", motor_is_running(cfg.motor) ? "yes" : "no");
        printf("Motor current: %d mA\n", motor_read_current_ma(cfg.motor));
    } else {
        printf("Usage: motor <on [speed]|off|status>\n");
    }
    return 0;
}

/* ==================== temp ==================== */

static int cmd_temp(int argc, char **argv)
{
    int deci_c = temp_sensor_read_deci_c(cfg.temp_sensor);
    if (deci_c == -9999) {
        printf("Temperature: READ ERROR\n");
    } else {
        printf("Temperature: %d.%d C\n", deci_c / 10, deci_c % 10);
    }

    int r = temp_sensor_read_resistance(cfg.temp_sensor);
    if (r > 0) {
        printf("NTC resistance: %d ohm\n", r);
    }
    return 0;
}

/* ==================== touch ==================== */

static int cmd_touch(int argc, char **argv)
{
    uint8_t state = touch_read_all();
    printf("Touch buttons:\n");
    printf("  SW1: %s\n", (state & 0x01) ? "PRESSED" : "off");
    printf("  SW2: %s\n", (state & 0x02) ? "PRESSED" : "off");
    printf("  SW3: %s\n", (state & 0x04) ? "PRESSED" : "off");
    printf("  SW4: %s\n", (state & 0x08) ? "PRESSED" : "off");
    return 0;
}

/* ==================== rgb ==================== */

static struct {
    struct arg_str *action;
    struct arg_int *brightness;
    struct arg_end *end;
} rgb_args;

static int cmd_rgb(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&rgb_args);
    if (nerrors != 0) { arg_print_errors(stderr, rgb_args.end, argv[0]); return 1; }

    const char *action = rgb_args.action->sval[0];

    if (strcmp(action, "on") == 0) {
        int brt = rgb_args.brightness->count > 0 ? rgb_args.brightness->ival[0] : 100;
        rgb_led_enable(true);
        rgb_led_set_brightness(brt);
        printf("RGB LED on, brightness %d%%\n", brt);
    } else if (strcmp(action, "off") == 0) {
        rgb_led_enable(false);
        printf("RGB LED off\n");
    } else {
        printf("Usage: rgb <on [brightness]|off>\n");
    }
    return 0;
}

/* ==================== display ==================== */

static struct {
    struct arg_str *text;
    struct arg_end *end;
} display_args;

static int cmd_display(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&display_args);
    if (nerrors != 0) { arg_print_errors(stderr, display_args.end, argv[0]); return 1; }

    if (display_args.text->count > 0) {
        display_show_text(0, display_args.text->sval[0]);
        printf("Display: %s\n", display_args.text->sval[0]);
    } else {
        display_clear();
        printf("Display cleared\n");
    }
    return 0;
}

/* ==================== status (all-in-one) ==================== */

static int cmd_status(int argc, char **argv)
{
    printf("=== Matcha Machine Status ===\n");

    int t = temp_sensor_read_deci_c(cfg.temp_sensor);
    printf("Temperature: %s\n", t == -9999 ? "ERROR" : "");
    if (t != -9999) printf("  %d.%d C\n", t / 10, t % 10);

    printf("Heater: %s\n", heater_is_active(cfg.heater) ? "ACTIVE" : "off");
    printf("Motor: %s", motor_is_running(cfg.motor) ? "RUNNING" : "off");
    if (motor_is_running(cfg.motor)) {
        printf(" (%d mA)", motor_read_current_ma(cfg.motor));
    }
    printf("\n");

    uint8_t touch = touch_read_all();
    printf("Touch: SW1=%d SW2=%d SW3=%d SW4=%d\n",
           !!(touch & 1), !!(touch & 2), !!(touch & 4), !!(touch & 8));

    return 0;
}

/* ==================== registration ==================== */

esp_err_t cli_init(const cli_config_t *config)
{
    cfg = *config;

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "matcha>";
    repl_config.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* heater command */
    heater_args.action = arg_str1(NULL, NULL, "<action>", "on/off/status");
    heater_args.power = arg_int0(NULL, NULL, "<percent>", "Power 0-100 (default 80)");
    heater_args.end = arg_end(2);
    esp_console_cmd_t heater_cmd = {
        .command = "heater",
        .help = "Control heater: heater <on [power]|off|status>",
        .func = cmd_heater,
        .argtable = &heater_args,
    };
    esp_console_cmd_register(&heater_cmd);

    /* motor command */
    motor_args.action = arg_str1(NULL, NULL, "<action>", "on/off/stop/speed/status");
    motor_args.speed = arg_int0(NULL, NULL, "<percent>", "Speed 0-100 (default 50)");
    motor_args.end = arg_end(2);
    esp_console_cmd_t motor_cmd = {
        .command = "motor",
        .help = "Control motor: motor <on [speed]|off|status>",
        .func = cmd_motor,
        .argtable = &motor_args,
    };
    esp_console_cmd_register(&motor_cmd);

    /* temp command */
    esp_console_cmd_t temp_cmd = {
        .command = "temp",
        .help = "Read temperature sensor",
        .func = cmd_temp,
    };
    esp_console_cmd_register(&temp_cmd);

    /* touch command */
    esp_console_cmd_t touch_cmd = {
        .command = "touch",
        .help = "Read touch button states",
        .func = cmd_touch,
    };
    esp_console_cmd_register(&touch_cmd);

    /* rgb command */
    rgb_args.action = arg_str1(NULL, NULL, "<action>", "on/off");
    rgb_args.brightness = arg_int0(NULL, NULL, "<percent>", "Brightness 0-100");
    rgb_args.end = arg_end(2);
    esp_console_cmd_t rgb_cmd = {
        .command = "rgb",
        .help = "Control RGB LED: rgb <on [brightness]|off>",
        .func = cmd_rgb,
        .argtable = &rgb_args,
    };
    esp_console_cmd_register(&rgb_cmd);

    /* display command */
    display_args.text = arg_str0(NULL, NULL, "<text>", "Text to show (omit to clear)");
    display_args.end = arg_end(1);
    esp_console_cmd_t display_cmd = {
        .command = "display",
        .help = "Show text on OLED or clear",
        .func = cmd_display,
        .argtable = &display_args,
    };
    esp_console_cmd_register(&display_cmd);

    /* status command */
    esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Show all sensor/actuator status",
        .func = cmd_status,
    };
    esp_console_cmd_register(&status_cmd);

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "CLI console started");
    return ESP_OK;
}
