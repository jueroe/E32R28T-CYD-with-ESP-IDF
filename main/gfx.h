/*
 * Minimal software drawing library for the SPI LCD test project.
 *
 * The canvas is a full screen RGB565 framebuffer.  The ILI9341 expects the high
 * byte of every 16 bit pixel first, so all pixel values are stored byte swapped
 * (see GFX_RGB565).  Always create colours with GFX_RGB565()/GFX_COLOR_*.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#define GFX_SWAP16(value)   ((uint16_t)((((uint16_t)(value) << 8) | ((uint16_t)(value) >> 8))))
#define GFX_RGB565(r, g, b) GFX_SWAP16((uint16_t)((((uint16_t)(r) & 0xF8) << 8) | \
                                                 (((uint16_t)(g) & 0xFC) << 3) | \
                                                 ((uint16_t)(b) >> 3)))

#define GFX_BLACK       GFX_RGB565(0x00, 0x00, 0x00)
#define GFX_WHITE       GFX_RGB565(0xFF, 0xFF, 0xFF)
#define GFX_RED         GFX_RGB565(0xFF, 0x00, 0x00)
#define GFX_GREEN       GFX_RGB565(0x00, 0xFF, 0x00)
#define GFX_BLUE        GFX_RGB565(0x00, 0x00, 0xFF)
#define GFX_CYAN        GFX_RGB565(0x00, 0xFF, 0xFF)
#define GFX_MAGENTA     GFX_RGB565(0xFF, 0x00, 0xFF)
#define GFX_YELLOW      GFX_RGB565(0xFF, 0xFF, 0x00)
#define GFX_ORANGE      GFX_RGB565(0xFF, 0x80, 0x00)
#define GFX_GRAY        GFX_RGB565(0x80, 0x80, 0x80)
#define GFX_DARK_GRAY   GFX_RGB565(0x30, 0x30, 0x30)
#define GFX_NAVY        GFX_RGB565(0x00, 0x20, 0x60)

/* Number of pixel lines sent to the panel per transfer */
#define GFX_FLUSH_BAND_LINES 24

typedef struct {
    uint16_t *pixels; /* RGB565, byte swapped for the panel, natively DMA capable */
    uint16_t *scratch; /* bounce buffer for partial (non full width) flushes */
    int width;
    int height;
} gfx_canvas_t;

/** Allocate the framebuffer and the flush bounce buffer. */
esp_err_t gfx_canvas_init(gfx_canvas_t *canvas, int width, int height);

/** Release the buffers of a canvas created with gfx_canvas_init(). */
void gfx_canvas_free(gfx_canvas_t *canvas);

void gfx_clear(gfx_canvas_t *canvas, uint16_t color);
void gfx_pixel(gfx_canvas_t *canvas, int x, int y, uint16_t color);
void gfx_fill_rect(gfx_canvas_t *canvas, int x, int y, int width, int height, uint16_t color);
void gfx_rect_outline(gfx_canvas_t *canvas, int x, int y, int width, int height, uint16_t color);
void gfx_hline(gfx_canvas_t *canvas, int x, int y, int width, uint16_t color);
void gfx_vline(gfx_canvas_t *canvas, int x, int y, int height, uint16_t color);
void gfx_line(gfx_canvas_t *canvas, int x0, int y0, int x1, int y1, uint16_t color);
void gfx_circle_filled(gfx_canvas_t *canvas, int cx, int cy, int radius, uint16_t color);
void gfx_gradient_h(gfx_canvas_t *canvas, int x, int y, int width, int height,
                    uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1);

/** Width in pixels that gfx_text() needs for `text`. */
int gfx_text_width(const char *text, int scale);

/**
 * Draw `text` (upper case only font, lower case input is folded) at x/y.
 * `opaque` fills the character cells with `background` first.
 */
void gfx_text(gfx_canvas_t *canvas, int x, int y, const char *text, uint16_t color,
              uint16_t background, int scale, bool opaque);

/** Send a rectangle of the canvas to the panel (clipped to the canvas). */
esp_err_t gfx_flush(gfx_canvas_t *canvas, esp_lcd_panel_handle_t panel,
                    int x, int y, int width, int height);

/** Send the whole canvas to the panel. */
esp_err_t gfx_flush_all(gfx_canvas_t *canvas, esp_lcd_panel_handle_t panel);
