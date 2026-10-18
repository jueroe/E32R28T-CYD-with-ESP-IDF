/*
 * WLAN station connection.
 *
 * The device logs into the access point from connect.h (CONNECT_WIFI_SSID).
 * All settings that belong to the site live in connect.h.
 *
 * The display works without a network, only the date and time line keeps the
 * fallback values when the connection fails.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Bring up the network stack and log into CONNECT_WIFI_SSID.
 *
 * Blocks until an IP address was obtained or the retries are used up.
 *
 * @param timeout_ms total time to wait for an address
 * @return true when the device has an IP address
 */
bool wifi_connect(uint32_t timeout_ms);

/** True while an IP address is assigned. */
bool wifi_is_connected(void);

/** Current IPv4 address as text, or "-" while not connected. */
const char *wifi_ip_address(void);

/** Log access point name, channel, signal strength and IP address. */
void wifi_log_link(void);
