/*
 * Template for main/connect.h - copy it and fill in your own values:
 *
 *     Copy-Item main\connect.example.h main\connect.h
 *
 * main/connect.h is listed in .gitignore, so the WiFi password never lands in
 * the repository.  Without the copy the build stops with
 * "connect.h: No such file or directory".
 */

#pragma once

#define CONNECT_WIFI_SSID       "your-network-name"
#define CONNECT_WIFI_PASSWORD   "your-password"
#define CONNECT_HOSTNAME        "raumthermometer"
#define CONNECT_WIFI_TIMEOUT_MS 20000

#define CONNECT_SNTP_SERVER     "de.pool.ntp.org"
#define CONNECT_SNTP_WAIT_MS    15000
#define CONNECT_TZ              "CET-1CEST,M3.5.0,M10.5.0/3"
#define CONNECT_TZ_NAME         "Europe/Berlin"

#define CONNECT_LATITUDE_TEXT   "48.41490736914154"
#define CONNECT_LONGITUDE_TEXT  "11.099721982616911"
#define CONNECT_SUN_API_URL \
    "https://api.sunrise-sunset.org/json?lat=" CONNECT_LATITUDE_TEXT \
    "&lng=" CONNECT_LONGITUDE_TEXT "&formatted=0"
#define CONNECT_SUN_TIMEOUT_MS  10000
#define CONNECT_SUN_REFRESH_S   21600
