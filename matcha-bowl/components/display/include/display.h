#pragma once

#include "esp_err.h"

/**
 * OLED display driver stub.
 *
 * SPI interface: SCLK=GPIO5, MOSI=GPIO17, DC=GPIO16.
 * Power control: Q6 P-FET via GPIO4.
 *
 * TODO: implement for specific OLED controller (SSD1306 / SH1106 / SSD1309).
 */

esp_err_t display_init(void);
void display_power(bool on);
void display_clear(void);
void display_show_text(int line, const char *text);
void display_show_temperature(int deci_c);
void display_show_status(const char *status);
