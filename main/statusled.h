/*
 * RGB status LED / ambilight.
 *
 * The three channels are driven by PWM (LEDC, same timer as the backlight), so
 * brightness and colour fades are possible:
 *
 *   night    -> rainbow fade as ambilight, one full turn per
 *               STATUSLED_AMBIENT_PERIOD_MS (red - yellow - green - cyan -
 *               blue - magenta - red)
 *   daytime  -> white at STATUSLED_IDLE_BRIGHTNESS percent
 *
 * The device hangs in an acrylic frame about 15 mm in front of a white wall, so
 * the LED lights that wall up from behind.
 *
 * statusled_set() switches the automatic behaviour off and owns the LED until
 * statusled_ambilight() is called again - the hardware test screen uses that for
 * its button feedback.
 */

#pragma once

#include <stdbool.h>

/* One full colour turn in night mode, and how bright the two modes are
 * (percent of the maximum PWM duty) */
#define STATUSLED_AMBIENT_PERIOD_MS   60000
#define STATUSLED_AMBIENT_BRIGHTNESS  100
#define STATUSLED_IDLE_BRIGHTNESS      30

/* Configure the three pins as PWM outputs, all colours off. */
void statusled_init(void);

/* Light every channel once so a broken colour is visible right after boot. */
void statusled_self_test(void);

/* Start the task that drives the LED. */
void statusled_start(void);

/**
 * Night mode on/off: true = rainbow fade, false = dimmed white.
 *
 * Also takes the LED back from a previous statusled_set() call.
 */
void statusled_ambilight(bool night);

/* Fixed colour at full brightness, low level = colour on (common anode).
 * Hands the LED over to the caller until the next statusled_ambilight(). */
void statusled_set(bool red, bool green, bool blue);
