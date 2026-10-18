/*
 * Display layout of the thermometer screen.
 *
 * Everything a human might want to tweak lives here.  Coordinates are in the
 * portrait orientation: (0,0) is the top left corner, x grows to the right up
 * to BOARD_SCREEN_WIDTH-1 (239), y grows down to BOARD_SCREEN_HEIGHT-1 (319).
 *
 * After a change: build, flash, and check the "bigdigit:" line in the serial
 * log - it reports the resulting digit height, the horizontal scale, the used
 * width and the positions.
 */

#pragma once

/* ------------------------------------------------------------------------- */
/* Temperature text: "xx,x"                                                  */
/* ------------------------------------------------------------------------- */

/* Left edge of the first big digit */
#define LAYOUT_TEMP_X               25

/* Baseline of the big digits, i.e. their bottom edge.  The comma tail and the
 * half size decimal digit end on this line as well. */
#define LAYOUT_TEMP_BASELINE_Y      160

/* The group is condensed horizontally until it fits this width.  The space to
 * the right of it is where the degree sign sits. */
#define LAYOUT_TEMP_MAX_WIDTH       190

/* Height of the two digits in front of the comma, in pixels */
#define LAYOUT_TEMP_DIGIT_HEIGHT    120

/* Strongest horizontal squeeze allowed.  If the width would need a stronger
 * squeeze, the digit height is reduced instead so the proportions stay
 * readable.  1.0 = never condense, 0.5 = very narrow. */
#define LAYOUT_TEMP_MIN_SQUEEZE     0.55f

/* Comma and decimal digit, relative to the digit height (0.5 = half size) */
#define LAYOUT_TEMP_DECIMAL_SCALE   0.5f

/* ------------------------------------------------------------------------- */
/* Degree sign                                                                */
/* ------------------------------------------------------------------------- */

/* Position of the top right corner of the drawn degree ink */
#define LAYOUT_TEMP_DEGREE_X        215
#define LAYOUT_TEMP_DEGREE_Y        40

/* Size of the degree sign, relative to LAYOUT_TEMP_DIGIT_HEIGHT.  The glyph is
 * small by design; 0.5 draws the ring at half the size the font would give at
 * digit height. */
#define LAYOUT_TEMP_DEGREE_SCALE    0.5f
