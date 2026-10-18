/*
 * Wallpaper handling: decodes a JPEG from the littlefs partition into the
 * canvas framebuffer.
 *
 * The table in background.h maps season and time of day to the file names, the
 * files themselves live at /littlefs/backgrounds/<season>/<time of day>/.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#include "gfx.h"

/* Seasons and times of day, used as indices into the table in background.h */
typedef enum {
    SEASON_SPRING = 0,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT
} season_t;

typedef enum {
    DAYTIME_MORNING = 0,
    DAYTIME_DAY,
    DAYTIME_EVENING,
    DAYTIME_NIGHT,
    DAYTIME_COUNT
} daytime_t;

/* Number of wallpapers per season and time of day (see background.h) */
#define WALLPAPER_PER_SLOT 3

/** Decode a JPEG file into the canvas. Does not touch the panel. */
esp_err_t wallpaper_load(gfx_canvas_t *canvas, const char *path);

/** Decode a JPEG and send the result to the panel. */
esp_err_t wallpaper_show(gfx_canvas_t *canvas, esp_lcd_panel_handle_t panel, const char *path);

/** Full path of one wallpaper of a slot, e.g. /littlefs/backgrounds/spring/day/2.jpg */
void wallpaper_slot_path(char *buffer, size_t size, season_t season, daytime_t daytime, int index);

/** Decode and show one wallpaper of a slot. */
esp_err_t wallpaper_show_slot(gfx_canvas_t *canvas, esp_lcd_panel_handle_t panel,
                              season_t season, daytime_t daytime, int index);

/** Random index 0..WALLPAPER_PER_SLOT-1. */
int wallpaper_random_index(void);

/** Season for a month (1..12), the meteorological seasons. */
season_t wallpaper_season_from_month(int month);

/** Time of day for an hour (0..23): morning 5..10, day 11..16, evening 17..21, night otherwise. */
daytime_t wallpaper_daytime_from_hour(int hour);
