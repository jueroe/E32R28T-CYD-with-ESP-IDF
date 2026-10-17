/*
 * Board definitions for the QDtech / LCDWiki "2.8inch ESP32-32E Display"
 * (SKU E32R28T, "Cheap Yellow Display" with ESP32-D0WD-V3, ILI9341V, XPT2046).
 *
 * Pin numbers are taken from the LCDWiki page "2.8inch ESP32-32E Display"
 * (ESP32 pin assignment table) which matches the board schematic.
 * Reference: https://www.lcdwiki.com/2.8inch_ESP32-32E_Display
 *
 * Change the values here if your board revision is wired differently - all
 * modules of this project only use the macros below.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"

/* ------------------------------------------------------------------------- */
/* LCD - ILI9341V, 4-wire SPI (own SPI bus)                                  */
/* ------------------------------------------------------------------------- */
#define BOARD_LCD_HOST            SPI2_HOST
#define BOARD_LCD_PIN_SCLK        GPIO_NUM_14
#define BOARD_LCD_PIN_MOSI        GPIO_NUM_13
#define BOARD_LCD_PIN_MISO        GPIO_NUM_12
#define BOARD_LCD_PIN_CS          GPIO_NUM_15
#define BOARD_LCD_PIN_DC          GPIO_NUM_2
#define BOARD_LCD_PIN_RST         GPIO_NUM_NC /* reset shares the ESP32 EN line */

#define BOARD_LCD_H_RES           240
#define BOARD_LCD_V_RES           320

/* 1 = 320x240 landscape (recommended for this board), 0 = 240x320 portrait.
 * If the picture appears rotated or mirrored, flip the two mirror flags. */
#define BOARD_LCD_LANDSCAPE       1
/* Landscape via MADCTL bit MV; on this unit both mirrors are needed for an
 * upright, non-mirrored picture.  Changing them also rotates the touch
 * coordinates, see the touch section below. */
#define BOARD_LCD_MIRROR_X        1
#define BOARD_LCD_MIRROR_Y        1

#if BOARD_LCD_LANDSCAPE
#define BOARD_SCREEN_WIDTH        BOARD_LCD_V_RES
#define BOARD_SCREEN_HEIGHT       BOARD_LCD_H_RES
#else
#define BOARD_SCREEN_WIDTH        BOARD_LCD_H_RES
#define BOARD_SCREEN_HEIGHT       BOARD_LCD_V_RES
#endif

/* Drop to 20 MHz if the picture shows noise, 40 MHz is fine on-board */
#define BOARD_LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)
/* 1 = BGR element order (correct for this panel, see the ILI9341 driver's
 * reference configuration), 0 = RGB.
 * If red and blue are swapped in the test pattern, toggle this value. */
#define BOARD_LCD_BGR             1
/* 1 = invert all colours.  0 is correct for this panel; set to 1 if the
 * picture looks like a photo negative. */
#define BOARD_LCD_INVERT_COLORS   0

/* ------------------------------------------------------------------------- */
/* LCD backlight, dimmed with LEDC (high level = backlight on)               */
/* ------------------------------------------------------------------------- */
#define BOARD_LCD_PIN_BACKLIGHT   GPIO_NUM_21
#define BOARD_BL_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BOARD_BL_LEDC_TIMER       LEDC_TIMER_0
#define BOARD_BL_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BOARD_BL_LEDC_RESOLUTION  LEDC_TIMER_10_BIT
#define BOARD_BL_LEDC_FREQ_HZ     20000
#define BOARD_BL_MAX_DUTY         ((1 << 10) - 1)

/* ------------------------------------------------------------------------- */
/* Touch - XPT2046 on its own SPI bus (not shared with the LCD)              */
/* ------------------------------------------------------------------------- */
#define BOARD_TOUCH_ENABLE        1
#define BOARD_TOUCH_HOST          SPI3_HOST
#define BOARD_TOUCH_PIN_SCLK      GPIO_NUM_25
#define BOARD_TOUCH_PIN_MOSI      GPIO_NUM_32
#define BOARD_TOUCH_PIN_MISO      GPIO_NUM_39
#define BOARD_TOUCH_PIN_CS        GPIO_NUM_33
#define BOARD_TOUCH_PIN_IRQ       GPIO_NUM_36 /* input only, not used by default */
#define BOARD_TOUCH_SPI_CLOCK_HZ  (1 * 1000 * 1000)

/*
 * The XPT2046 is read as a raw 12 bit ADC value (0..4095) and mapped to screen
 * coordinates here.
 *
 * On this panel the touch axes are rotated by 90 degrees against the display
 * and both raw values count the other way round (large raw value = small
 * screen coordinate), so swap_xy and both mirror flags are enabled.
 *
 * Measured on this unit when touching the four corners of the screen (values
 * logged by main.c as "TOUCH: RAW x/y"):
 *     top left     3791/3714        top right     3808/218
 *     bottom left   283/3704        bottom right   323/214
 * so raw_x grows towards the top edge (283 = bottom, 3808 = top) and raw_y
 * towards the left edge (214 = right, 3714 = left).  Touching the same corner
 * again varies these values by a few dozen counts, hence the window below is
 * rounded slightly outwards; readings outside it are clamped, so the outermost
 * pixel rows and columns stay reachable.
 *
 * With these values the four corners map to (0,1) (319,0) (1,239) (319,238)
 * and the screen centre to (160,120).
 *
 * Because of the swap, the X macros calibrate the screen Y axis and the Y
 * macros the screen X axis.  To re-calibrate: touch each corner, read the RAW
 * values from the serial log and put the outermost values into the macros
 * below.  These flags also depend on the display orientation
 * (BOARD_LCD_LANDSCAPE / BOARD_LCD_MIRROR_*) - if that is changed, the mirror
 * flags here normally have to be flipped too.
 */
#define BOARD_TOUCH_SWAP_XY       1
#define BOARD_TOUCH_MIRROR_X      1
#define BOARD_TOUCH_MIRROR_Y      1
/* raw_x -> screen Y: 300 = bottom edge, 3805 = top edge */
#define BOARD_TOUCH_RAW_MIN_X     300
#define BOARD_TOUCH_RAW_MAX_X     3805
/* raw_y -> screen X: 215 = right edge, 3712 = left edge */
#define BOARD_TOUCH_RAW_MIN_Y     215
#define BOARD_TOUCH_RAW_MAX_Y     3712
/*
 * Raw value of a released panel (used to filter noise).  Must match
 * CONFIG_XPT2046_Z_THRESHOLD in sdkconfig/sdkconfig.defaults, which makes the
 * driver discard lighter readings before they reach us.
 *
 * Measured on this unit: the untouched panel reads near 0, the lightest corner
 * touch (bottom left, with a stylus) around 430, other positions 500..2700.
 * 250 therefore keeps a good margin on both sides; raise this if stray dots
 * appear without touching, lower it if light touches are ignored.
 */
#define BOARD_TOUCH_Z_THRESHOLD   250

/* ------------------------------------------------------------------------- */
/* RGB status LED: common anode, one chip per colour, low level = on         */
/* ------------------------------------------------------------------------- */
/* Other CYD variants drive the red chip from GPIO4 (which is the audio enable
 * on this board), so pin maps of different revisions must not be mixed.
 * main.c flashes all three channels once at startup and then uses
 * green = running, blue = while the test pattern is redrawn. */
#define BOARD_LED_PIN_R           GPIO_NUM_22
#define BOARD_LED_PIN_G           GPIO_NUM_16
#define BOARD_LED_PIN_B           GPIO_NUM_17

/* ------------------------------------------------------------------------- */
/* Keys                                                                      */
/* ------------------------------------------------------------------------- */
#define BOARD_PIN_BOOT            GPIO_NUM_0 /* "BOOT" key, low when pressed */

/* ------------------------------------------------------------------------- */
/* Not used by this test, listed for completeness                            */
/* ------------------------------------------------------------------------- */
/* microSD (SPI3):  CS = GPIO5, MOSI = GPIO23, SCLK = GPIO18, MISO = GPIO19   */
/* Audio:           enable = GPIO4 (low active), output = GPIO26             */
