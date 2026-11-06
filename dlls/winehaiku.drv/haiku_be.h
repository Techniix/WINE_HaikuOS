/*
 * Haiku driver: interface to the Haiku (Be API) glue code
 *
 * Copyright 2026 Techniix
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_HAIKU_BE_H
#define __WINE_HAIKU_BE_H

/* This header is shared between the C driver code (which uses Wine headers)
 * and haiku_be.cpp (which uses Haiku headers), so it only uses plain types. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum haiku_event_type
{
    HAIKU_EVENT_MOUSE,          /* x, y (screen), buttons, old_buttons */
    HAIKU_EVENT_WHEEL,          /* wheel_x, wheel_y */
    HAIKU_EVENT_KEY_DOWN,       /* key */
    HAIKU_EVENT_KEY_UP,         /* key */
    HAIKU_EVENT_CLOSE,          /* close requested by the user */
    HAIKU_EVENT_ACTIVATE,       /* active */
    HAIKU_EVENT_SCREEN_CHANGED, /* screen mode or layout changed */
};

/* Haiku window threads are not Wine threads, so they never call into Wine;
 * they write these to a pipe that a Wine thread reads (see haikudrv_main.c).
 * Keep it small: pipe writes of up to PIPE_BUF bytes are atomic. */
struct haiku_event
{
    int32_t   type;
    int32_t   x, y;
    uint32_t  buttons;
    uint32_t  old_buttons;
    int32_t   key;
    int32_t   active;
    float     wheel_x, wheel_y;
    uint64_t  hwnd;
};

/* mouse buttons, as reported by Haiku */
#define HAIKU_BUTTON_PRIMARY    0x1
#define HAIKU_BUTTON_SECONDARY  0x2
#define HAIKU_BUTTON_TERTIARY   0x4

typedef struct haiku_window haiku_window;  /* opaque, a WineWindow (BWindow) */

int  haiku_be_init( int event_fd );
haiku_window *haiku_window_create( uint64_t hwnd, int popup );
void haiku_window_destroy( haiku_window *win );
void haiku_window_set_frame( haiku_window *win, int x, int y, int width, int height );
void haiku_window_show( haiku_window *win, int show, int activate );
void haiku_window_set_title( haiku_window *win, const char *utf8 );
/* bits: 32-bit top-down surface of width x height, placed at (origin_x, origin_y)
 * in window coordinates; left/top/right/bottom: dirty rect in surface coordinates */
void haiku_window_update( haiku_window *win, const void *bits, int stride, int width, int height,
                          int origin_x, int origin_y, int left, int top, int right, int bottom );
void haiku_get_screen_info( int *width, int *height, int *bpp, int *refresh );
int  haiku_get_mouse( int *x, int *y );
void haiku_set_mouse( int x, int y );

#ifdef __cplusplus
}
#endif

#endif /* __WINE_HAIKU_BE_H */
