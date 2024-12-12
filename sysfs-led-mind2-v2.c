/** @file sysfs-led-mind2-v2.c
 *
 * mce-plugin-libhybris - Libhybris plugin for Mode Control Entity
 * <p>
 * Copyright (c) 2024 Jollyboys Ltd.
 * <p>
 * @author Simo Piiroinen <simo.piiroinen@jolla.com>
 *
 * mce-plugin-libhybris is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * mce-plugin-libhybris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with mce-plugin-libhybris; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "sysfs-led-mind2-v2.h"

#include "sysfs-led-util.h"
#include "sysfs-val.h"
#include "plugin-config.h"

#include <stdio.h>
#include <string.h>

#include <glib.h>

/* ========================================================================= *
 * Constants
 * ========================================================================= */

#define DIFFERENTIATE_OUTER_LED 0 /* Debugging: when nonzero, outer led is
                                   * made to use color that differs from
                                   * what mce has requested. */

enum {
    MIND2V2_LED_INNER,
    MIND2V2_LED_OUTER,
    MIND2V2_LED_COUNT
};

/* Brightness range to be used in "led on" situations */
#define MIND2V2_MIN_BRIGHTNESS  1
#define MIND2V2_MAX_BRIGHTNESS 63

/* ========================================================================= *
 * Types
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LED_PATHS_MIND2V2
 * ------------------------------------------------------------------------- */

typedef struct {
    const char *red;
    const char *green;
    const char *blue;

    const char *red_max;
    const char *green_max;
    const char *blue_max;
} led_paths_mind2v2_t;

/* ------------------------------------------------------------------------- *
 * LEDS_PATHS_MIND2V2
 * ------------------------------------------------------------------------- */

typedef struct {
    const char          *power;
    led_paths_mind2v2_t  led[MIND2V2_LED_COUNT];
} leds_paths_mind2v2_t;

/* ------------------------------------------------------------------------- *
 * LED_STATE_MIND2V2
 * ------------------------------------------------------------------------- */

typedef struct {
    sysfsval_t *cached_red;        // 0 - N
    sysfsval_t *cached_green;      // 0 - N
    sysfsval_t *cached_blue;       // 0 - N

    sysfsval_t *cached_red_max;    // N
    sysfsval_t *cached_green_max;  // N
    sysfsval_t *cached_blue_max;   // N
} led_state_mind2v2_t;

/* ------------------------------------------------------------------------- *
 * LEDS_STATE_MIND2V2
 * ------------------------------------------------------------------------- */

typedef struct {
    sysfsval_t          *cached_power; // 0 / 1
    led_state_mind2v2_t  led[MIND2V2_LED_COUNT];
} leds_state_mind2v2_t;

/* ========================================================================= *
 * LED_STATE_MIND2V2
 * ========================================================================= */

static void
led_state_mind2v2_init(led_state_mind2v2_t *self)
{
    self->cached_red        = sysfsval_create();
    self->cached_green      = sysfsval_create();
    self->cached_blue       = sysfsval_create();

    self->cached_red_max    = sysfsval_create();
    self->cached_green_max  = sysfsval_create();
    self->cached_blue_max   = sysfsval_create();
}

static void
led_state_mind2v2_quit(led_state_mind2v2_t *self)
{
    sysfsval_delete_at(&self->cached_red);
    sysfsval_delete_at(&self->cached_green);
    sysfsval_delete_at(&self->cached_blue);

    sysfsval_delete_at(&self->cached_red_max);
    sysfsval_delete_at(&self->cached_green_max);
    sysfsval_delete_at(&self->cached_blue_max);
}

static void
led_state_mind2v2_close(led_state_mind2v2_t *self)
{
    sysfsval_close(self->cached_red);
    sysfsval_close(self->cached_green);
    sysfsval_close(self->cached_blue);

    sysfsval_close(self->cached_red_max);
    sysfsval_close(self->cached_green_max);
    sysfsval_close(self->cached_blue_max);
}

static bool
led_state_mind2v2_brobe(led_state_mind2v2_t *self, const led_paths_mind2v2_t *paths)
{
    bool res = false;

    if( !sysfsval_open_rw(self->cached_red,   paths->red)   ||
        !sysfsval_open_rw(self->cached_green, paths->green) ||
        !sysfsval_open_rw(self->cached_blue,  paths->blue) )
        goto cleanup;

    if( !sysfsval_open_ro(self->cached_red_max,   paths->red_max)   ||
        !sysfsval_open_ro(self->cached_green_max, paths->green_max) ||
        !sysfsval_open_ro(self->cached_blue_max,  paths->blue_max) )
        goto cleanup;

    if( !sysfsval_refresh(self->cached_red_max)   ||
        !sysfsval_refresh(self->cached_green_max) ||
        !sysfsval_refresh(self->cached_blue_max) )
        goto cleanup;

    if( sysfsval_get(self->cached_red_max)   <= 0 ||
        sysfsval_get(self->cached_green_max) <= 0 ||
        sysfsval_get(self->cached_blue_max)  <= 0 )
        goto cleanup;

    res = true;

cleanup:

    /* In any case: max_brightness files can be closed */
    sysfsval_close(self->cached_red_max);
    sysfsval_close(self->cached_green_max);
    sysfsval_close(self->cached_blue_max);

    if( !res )
        led_state_mind2v2_close(self);

    return res;
}

static int
led_state_mind2v2_scale_value(int val, int max_val)
{
    if( val <= 0 ) {
        val = 0;
    }
    else {
        if( max_val > MIND2V2_MAX_BRIGHTNESS )
            max_val = MIND2V2_MAX_BRIGHTNESS;
        val = led_util_trans(val, 1, 255, MIND2V2_MIN_BRIGHTNESS, max_val);
    }
    return val;
}

static void
led_state_mind2v2_set_value(led_state_mind2v2_t *self, int r, int g, int b)
{
    r = led_state_mind2v2_scale_value(r, sysfsval_get(self->cached_red_max));
    g = led_state_mind2v2_scale_value(g, sysfsval_get(self->cached_green_max));
    b = led_state_mind2v2_scale_value(b, sysfsval_get(self->cached_blue_max));

    sysfsval_set(self->cached_red,   r);
    sysfsval_set(self->cached_green, g);
    sysfsval_set(self->cached_blue,  b);
}

static bool
led_state_mind2v2_is_active(led_state_mind2v2_t *self)
{
    return (sysfsval_get(self->cached_red)   > 0 ||
            sysfsval_get(self->cached_green) > 0 ||
            sysfsval_get(self->cached_blue)  > 0);
}

/* ========================================================================= *
 * LEDS_STATE_MIND2V2
 * ========================================================================= */

static bool
leds_state_mind2v2_valid_index(int idx)
{
    return 0 <= idx && idx < MIND2V2_LED_COUNT;
}

static void
leds_state_mind2v2_init(leds_state_mind2v2_t *self)
{
    self->cached_power = sysfsval_create();

    for( int idx = 0; idx < MIND2V2_LED_COUNT; ++idx )
        led_state_mind2v2_init(&self->led[idx]);
}

static void
leds_state_mind2v2_quit(leds_state_mind2v2_t *self)
{
    sysfsval_delete_at(&self->cached_power);

    for( int idx = 0; idx < MIND2V2_LED_COUNT; ++idx )
        led_state_mind2v2_quit(&self->led[idx]);
}

static void
leds_state_mind2v2_close(leds_state_mind2v2_t *self)
{
    sysfsval_close(self->cached_power);

    for( int idx = 0; idx < MIND2V2_LED_COUNT; ++idx )
        led_state_mind2v2_close(&self->led[idx]);
}

static bool
leds_state_mind2v2_probe(leds_state_mind2v2_t *self, const leds_paths_mind2v2_t *paths)
{
    bool res = false;

    if( !sysfsval_open_rw(self->cached_power, paths->power) )
        goto cleanup;

    for( int idx = 0; idx < MIND2V2_LED_COUNT; ++idx )
        if( !led_state_mind2v2_brobe(&self->led[idx], &paths->led[idx]) )
            goto cleanup;

    res = true;

cleanup:
    if( !res )
        leds_state_mind2v2_close(self);

    return res;
}

static void
leds_state_mind2v2_update_power(leds_state_mind2v2_t *self)
{
    bool power = false;

    for( int idx = 0; idx < MIND2V2_LED_COUNT; ++idx )
        if( (power = led_state_mind2v2_is_active(&self->led[idx])) )
            break;

    if( power )
        sysfsval_set(self->cached_power, true);
}

static void
leds_state_mind2v2_set_value(leds_state_mind2v2_t *self, int idx, int r, int g, int b)
{
    if( leds_state_mind2v2_valid_index(idx) )
        led_state_mind2v2_set_value(&self->led[idx], r, g, b);
}

/* ========================================================================= *
 * LED_CONTROL_MIND2V2
 * ========================================================================= */

static void
led_control_mind2v2_value_cb(void *data, int r, int g, int b)
{
    leds_state_mind2v2_t *state = data;

    leds_state_mind2v2_set_value(state, MIND2V2_LED_INNER, r, g, b);
#if DIFFERENTIATE_OUTER_LED
    leds_state_mind2v2_set_value(state, MIND2V2_LED_OUTER, g, b, r);
#else
    leds_state_mind2v2_set_value(state, MIND2V2_LED_OUTER, r, g, b);
#endif
    leds_state_mind2v2_update_power(state);
}

static void
led_control_mind2v2_close_cb(void *data)
{
    leds_state_mind2v2_t *state = data;

    leds_state_mind2v2_quit(state);
}

static bool
led_control_mind2v2_static_probe(leds_state_mind2v2_t *state)
{
    static const leds_paths_mind2v2_t paths = {
        .power = "/sys/class/leds/Led/brightness",
        .led   = {
            {
                .red        = "/sys/class/leds/Ired/brightness",
                .green      = "/sys/class/leds/Igreen/brightness",
                .blue       = "/sys/class/leds/Iblue/brightness",

                .red_max    = "/sys/class/leds/Ired/max_brightness",
                .green_max  = "/sys/class/leds/Igreen/max_brightness",
                .blue_max   = "/sys/class/leds/Iblue/max_brightness",
            },
            {
                .red        = "/sys/class/leds/Ored/brightness",
                .green      = "/sys/class/leds/Ogreen/brightness",
                .blue       = "/sys/class/leds/Oblue/brightness",

                .red_max    = "/sys/class/leds/Ored/max_brightness",
                .green_max  = "/sys/class/leds/Ogreen/max_brightness",
                .blue_max   = "/sys/class/leds/Oblue/max_brightness",
            }
        }
    };

    return leds_state_mind2v2_probe(state, &paths);
}

static bool
led_control_mind2v2_dynamic_probe(leds_state_mind2v2_t *state)
{
    // XXX: No configuration tweaks for now
    (void)state;
    return false;
}

bool
led_control_mind2v2_probe(led_control_t *self)
{
    static leds_state_mind2v2_t state = { };

    bool res = false;

    leds_state_mind2v2_init(&state);

    self->name   = "mind2v2";
    self->data   = &state;
    self->enable = NULL;
    self->blink  = NULL;
    self->value  = led_control_mind2v2_value_cb;
    self->close  = led_control_mind2v2_close_cb;

    self->can_breathe = true;
    self->breath_type = LED_RAMP_SINE;

    if( self->use_config )
        res = led_control_mind2v2_dynamic_probe(&state);

    if( !res )
        res = led_control_mind2v2_static_probe(&state);

    if( !res )
        leds_state_mind2v2_quit(&state);

    return res;
}
