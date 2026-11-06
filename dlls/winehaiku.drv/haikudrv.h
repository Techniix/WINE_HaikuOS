/*
 * Haiku driver definitions
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

#ifndef __WINE_HAIKUDRV_H
#define __WINE_HAIKUDRV_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <stdarg.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "ntgdi.h"
#include "ntuser.h"
#include "wine/gdi_driver.h"
#include "wine/rbtree.h"

#include "unixlib.h"
#include "haiku_be.h"

/* messages posted from the event thread to window threads */
enum haikudrv_window_message
{
    WM_HAIKU_SET_FOREGROUND = WM_WINE_FIRST_DRIVER_MSG,
    WM_HAIKU_DISPLAY_CHANGED,
};

/* per-window driver data, for top-level windows only */
struct haiku_win_data
{
    struct rb_entry     entry;
    HWND                hwnd;
    haiku_window       *window;   /* Haiku window, created when the window gets a surface */
    struct window_rects rects;
};

extern struct haiku_win_data *haiku_win_data_get( HWND hwnd );
extern void haiku_win_data_release( struct haiku_win_data *data );

extern void haiku_handle_event( const struct haiku_event *event );
extern UINT haiku_key_to_scan( int key );

extern BOOL HAIKU_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect,
                                       struct window_surface **surface );
extern LRESULT HAIKU_DesktopWindowProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
extern void HAIKU_DestroyWindow( HWND hwnd );
extern BOOL HAIKU_GetCursorPos( POINT *pos );
extern BOOL HAIKU_SetCursorPos( INT x, INT y );
extern void HAIKU_SetWindowText( HWND hwnd, LPCWSTR text );
extern UINT HAIKU_UpdateDisplayDevices( const struct gdi_device_manager *device_manager, void *param );
extern LRESULT HAIKU_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
extern BOOL HAIKU_WindowPosChanging( HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects );
extern void HAIKU_WindowPosChanged( HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                                    const struct window_rects *new_rects, struct window_surface *surface );

#endif /* __WINE_HAIKUDRV_H */
