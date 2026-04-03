#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include <stdbool.h>

/**
 * Ethernet driver — LAN8720A PHY via RMII interface.
 *
 * Initializes the ESP32-P4 internal EMAC + external LAN8720A PHY.
 * Provides DHCP client and event handling for link up/down.
 */

esp_err_t ethernet_init(void);
bool      ethernet_is_connected(void);

/** Get the assigned IP address as a string. Returns "" if not connected. */
const char *ethernet_get_ip(void);
