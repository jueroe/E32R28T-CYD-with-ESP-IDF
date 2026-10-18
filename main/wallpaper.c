#include "wallpaper.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "jpeg_decoder.h"

#include "background.h"

static const char *TAG = "wallpaper";

static const char *const s_season_names[SEASON_COUNT] = {"spring", "summer", "autumn", "winter"};
static const char *const s_daytime_names[DAYTIME_COUNT] = {"morning", "day", "evening", "night"};

esp_err_t wallpaper_load(gfx_canvas_t *canvas, const char *path)
{
    struct stat file_info;
    if (stat(path, &file_info) != 0) {
        ESP_LOGE(TAG, "%s not found", path);
        return ESP_ERR_NOT_FOUND;
    }
    if (file_info.st_size <= 0) {
        ESP_LOGE(TAG, "%s is empty", path);
        return ESP_ERR_INVALID_SIZE;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "cannot open %s", path);
        return ESP_FAIL;
    }

    const size_t file_size = (size_t)file_info.st_size;
    uint8_t *compressed = heap_caps_malloc(file_size, MALLOC_CAP_8BIT);
    if (compressed == NULL) {
        ESP_LOGE(TAG, "no memory for the %u byte JPEG", (unsigned)file_size);
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    const size_t read = fread(compressed, 1, file_size, file);
    fclose(file);
    if (read != file_size) {
        ESP_LOGE(TAG, "short read on %s (%u of %u bytes)", path, (unsigned)read, (unsigned)file_size);
        heap_caps_free(compressed);
        return ESP_FAIL;
    }

    esp_jpeg_image_cfg_t config = {
        .indata = compressed,
        .indata_size = (uint32_t)file_size,
        .outbuf = (uint8_t *)canvas->pixels,
        .outbuf_size = (uint32_t)((size_t)canvas->width * canvas->height * sizeof(uint16_t)),
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            /* the panel wants the high byte of every pixel first, which is the
             * byte order this canvas stores (see GFX_RGB565) */
            .swap_color_bytes = 1,
        },
    };

    esp_jpeg_image_output_t output;
    const esp_err_t err = esp_jpeg_decode(&config, &output);
    heap_caps_free(compressed); /* the decoder is blocking, the input is no longer needed */

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "decoding %s failed: %s", path, esp_err_to_name(err));
        return err;
    }
    if (output.width != canvas->width || output.height != canvas->height) {
        ESP_LOGE(TAG, "%s is %ux%u, the canvas is %dx%d", path, output.width, output.height,
                 canvas->width, canvas->height);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "%s: %ux%u, %u bytes JPEG -> RGB565", path, output.width, output.height,
             (unsigned)file_size);
    return ESP_OK;
}

esp_err_t wallpaper_show(gfx_canvas_t *canvas, esp_lcd_panel_handle_t panel, const char *path)
{
    const esp_err_t err = wallpaper_load(canvas, path);
    if (err != ESP_OK) {
        return err;
    }
    return gfx_flush_all(canvas, panel);
}

void wallpaper_slot_path(char *buffer, size_t size, season_t season, daytime_t daytime, int index)
{
    if (season >= SEASON_COUNT || daytime >= DAYTIME_COUNT || index < 0 || index >= WALLPAPER_PER_SLOT) {
        snprintf(buffer, size, "%s", "");
        return;
    }
    snprintf(buffer, size, "%s%s/%s/%s", PATH_TO_BACKGROUND_IMAGES, s_season_names[season],
             s_daytime_names[daytime], backgrounds[season][daytime][index].filename);
}

esp_err_t wallpaper_show_slot(gfx_canvas_t *canvas, esp_lcd_panel_handle_t panel,
                              season_t season, daytime_t daytime, int index)
{
    char path[128];
    wallpaper_slot_path(path, sizeof(path), season, daytime, index);
    if (path[0] == '\0') {
        ESP_LOGE(TAG, "invalid wallpaper slot %d/%d/%d", season, daytime, index);
        return ESP_ERR_INVALID_ARG;
    }
    return wallpaper_show(canvas, panel, path);
}

int wallpaper_random_index(void)
{
    return (int)(esp_random() % WALLPAPER_PER_SLOT);
}

season_t wallpaper_season_from_month(int month)
{
    if (month < 1 || month > 12) {
        return SEASON_SUMMER;
    }
    if (month >= 3 && month <= 5) {
        return SEASON_SPRING;
    }
    if (month >= 6 && month <= 8) {
        return SEASON_SUMMER;
    }
    if (month >= 9 && month <= 11) {
        return SEASON_AUTUMN;
    }
    return SEASON_WINTER;
}

daytime_t wallpaper_daytime_from_hour(int hour)
{
    if (hour >= 5 && hour <= 10) {
        return DAYTIME_MORNING;
    }
    if (hour >= 11 && hour <= 16) {
        return DAYTIME_DAY;
    }
    if (hour >= 17 && hour <= 21) {
        return DAYTIME_EVENING;
    }
    return DAYTIME_NIGHT;
}
