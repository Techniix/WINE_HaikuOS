/*
 * Haiku driver window management
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdlib.h>

#include "haikudrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);

static int win_data_cmp( const void *key, const struct rb_entry *entry )
{
    HWND hwnd = (HWND)key;
    HWND entry_hwnd = RB_ENTRY_VALUE( entry, struct haiku_win_data, entry )->hwnd;
    return hwnd < entry_hwnd ? -1 : hwnd > entry_hwnd ? 1 : 0;
}

static pthread_mutex_t win_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct rb_tree win_data_tree = { win_data_cmp };

/* Returns the data with win_data_mutex held, or NULL. */
struct haiku_win_data *haiku_win_data_get( HWND hwnd )
{
    struct rb_entry *entry;

    pthread_mutex_lock( &win_data_mutex );
    if ((entry = rb_get( &win_data_tree, hwnd )))
        return RB_ENTRY_VALUE( entry, struct haiku_win_data, entry );
    pthread_mutex_unlock( &win_data_mutex );
    return NULL;
}

void haiku_win_data_release( struct haiku_win_data *data )
{
    if (data) pthread_mutex_unlock( &win_data_mutex );
}

static BOOL is_toplevel( HWND hwnd )
{
    HWND desktop = NtUserGetDesktopWindow();
    return hwnd != desktop && NtUserGetAncestor( hwnd, GA_PARENT ) == desktop;
}

/* Popups (menus, tooltips, dropdowns) float above their owner and must not
 * take the focus away from it. */
static BOOL is_popup( HWND hwnd )
{
    UINT style = NtUserGetWindowLongW( hwnd, GWL_STYLE );
    UINT ex_style = NtUserGetWindowLongW( hwnd, GWL_EXSTYLE );

    if (ex_style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) return TRUE;
    if ((style & WS_POPUP) && (style & WS_CAPTION) != WS_CAPTION && !(ex_style & WS_EX_APPWINDOW))
        return TRUE;
    return FALSE;
}

static void set_window_title( haiku_window *window, const WCHAR *text )
{
    DWORD len = (lstrlenW( text ) + 1) * sizeof(WCHAR), utf8_len;
    char *utf8;

    if (RtlUnicodeToUTF8N( NULL, 0, &utf8_len, text, len ) || !(utf8 = malloc( utf8_len ))) return;
    RtlUnicodeToUTF8N( utf8, utf8_len, &utf8_len, text, len );
    haiku_window_set_title( window, utf8 );
    free( utf8 );
}


/***********************************************************************
 *           window surface
 */

struct haiku_window_surface
{
    struct window_surface header;
};

static void haiku_surface_set_clip( struct window_surface *surface, const RECT *rects, UINT count )
{
    /* the whole surface is always copied, clipping is done by the window system */
}

static BOOL haiku_surface_flush( struct window_surface *surface, const RECT *rect, const RECT *dirty,
                                 const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                 const BITMAPINFO *shape_info, const void *shape_bits )
{
    struct haiku_win_data *data;
    int width = color_info->bmiHeader.biWidth, height = abs( color_info->bmiHeader.biHeight );

    TRACE( "hwnd %p rect %s dirty %s\n", surface->hwnd, wine_dbgstr_rect( rect ), wine_dbgstr_rect( dirty ));

    if (!(data = haiku_win_data_get( surface->hwnd ))) return TRUE;
    if (data->window)
        haiku_window_update( data->window, color_bits, width * 4, width, height, rect->left, rect->top,
                             dirty->left, dirty->top, dirty->right, dirty->bottom );
    haiku_win_data_release( data );
    return TRUE;
}

static void haiku_surface_destroy( struct window_surface *surface )
{
    TRACE( "surface %p\n", surface );
}

static const struct window_surface_funcs haiku_surface_funcs =
{
    .size = sizeof(struct haiku_window_surface),
    .set_clip = haiku_surface_set_clip,
    .flush = haiku_surface_flush,
    .destroy = haiku_surface_destroy,
};

static struct window_surface *haiku_surface_create( HWND hwnd, const RECT *rect )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    int width = rect->right - rect->left, height = rect->bottom - rect->top;

    memset( info, 0, sizeof(*info) );
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biSizeImage   = width * height * 4;
    info->bmiHeader.biCompression = BI_RGB;

    return window_surface_create( &haiku_surface_funcs, hwnd, rect, info, 0 );
}

/***********************************************************************
 *           HAIKU_CreateWindowSurface
 */
BOOL HAIKU_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect,
                                struct window_surface **surface )
{
    struct window_surface *previous;
    struct haiku_win_data *data;

    TRACE( "hwnd %p layered %u rect %s\n", hwnd, layered, wine_dbgstr_rect( surface_rect ));

    if ((previous = *surface) && previous->funcs == &haiku_surface_funcs) return TRUE;
    if (!(data = haiku_win_data_get( hwnd ))) return TRUE; /* use default surface */
    haiku_win_data_release( data );

    if (previous) window_surface_release( previous );
    *surface = haiku_surface_create( hwnd, surface_rect );
    return TRUE;
}


/***********************************************************************
 *           HAIKU_WindowPosChanging
 */
BOOL HAIKU_WindowPosChanging( HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects )
{
    struct haiku_win_data *data;

    TRACE( "hwnd %p flags %04x rects %s\n", hwnd, swp_flags, debugstr_window_rects( rects ));

    if (!is_toplevel( hwnd )) return FALSE;
    if ((data = haiku_win_data_get( hwnd )))
    {
        haiku_win_data_release( data );
        return TRUE;
    }

    if (!(data = calloc( 1, sizeof(*data) ))) return FALSE;
    data->hwnd = hwnd;
    data->rects = *rects;

    pthread_mutex_lock( &win_data_mutex );
    if (rb_put( &win_data_tree, hwnd, &data->entry )) free( data ); /* already added by another thread */
    pthread_mutex_unlock( &win_data_mutex );
    return TRUE;
}

/***********************************************************************
 *           HAIKU_WindowPosChanged
 */
void HAIKU_WindowPosChanged( HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                             const struct window_rects *new_rects, struct window_surface *surface )
{
    struct haiku_win_data *data;
    BOOL visible = (NtUserGetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE) && !(swp_flags & SWP_HIDEWINDOW);
    BOOL popup = is_popup( hwnd );
    const RECT *rect = &new_rects->visible;
    WCHAR title[1024];

    TRACE( "hwnd %p flags %04x rects %s surface %p\n", hwnd, swp_flags, debugstr_window_rects( new_rects ), surface );

    if (!NtUserInternalGetWindowText( hwnd, title, ARRAY_SIZE(title) )) title[0] = 0;

    if (!(data = haiku_win_data_get( hwnd ))) return;
    data->rects = *new_rects;

    if (!surface)
    {
        if (data->window) haiku_window_show( data->window, FALSE, FALSE );
        haiku_win_data_release( data );
        return;
    }

    if (!data->window && (data->window = haiku_window_create( (UINT_PTR)hwnd, popup )))
        set_window_title( data->window, title );

    if (data->window)
    {
        haiku_window_set_frame( data->window, rect->left, rect->top,
                                rect->right - rect->left, rect->bottom - rect->top );
        haiku_window_show( data->window, visible, visible && !popup && !(swp_flags & SWP_NOACTIVATE) );
    }
    haiku_win_data_release( data );
}

/***********************************************************************
 *           HAIKU_DestroyWindow
 */
void HAIKU_DestroyWindow( HWND hwnd )
{
    struct haiku_win_data *data;

    TRACE( "hwnd %p\n", hwnd );

    if (!(data = haiku_win_data_get( hwnd ))) return;
    rb_remove( &win_data_tree, &data->entry );
    haiku_win_data_release( data );

    if (data->window) haiku_window_destroy( data->window );
    free( data );
}

/***********************************************************************
 *           HAIKU_SetWindowText
 */
void HAIKU_SetWindowText( HWND hwnd, LPCWSTR text )
{
    struct haiku_win_data *data;

    TRACE( "hwnd %p text %s\n", hwnd, debugstr_w( text ));

    if (!(data = haiku_win_data_get( hwnd ))) return;
    if (data->window) set_window_title( data->window, text );
    haiku_win_data_release( data );
}

/***********************************************************************
 *           HAIKU_WindowMessage
 */
LRESULT HAIKU_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_HAIKU_SET_FOREGROUND:
        NtUserSetForegroundWindowInternal( hwnd );
        return 0;
    case WM_HAIKU_DISPLAY_CHANGED:
        NtUserCallNoParam( NtUserCallNoParam_DisplayModeChanged );
        return 0;
    default:
        FIXME( "got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp );
        return 0;
    }
}

/***********************************************************************
 *           HAIKU_DesktopWindowProc
 */
LRESULT HAIKU_DesktopWindowProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    return NtUserMessageCall( hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE );
}

/***********************************************************************
 *           HAIKU_GetCursorPos
 */
BOOL HAIKU_GetCursorPos( POINT *pos )
{
    int x, y;

    if (!haiku_get_mouse( &x, &y )) return FALSE;
    pos->x = x;
    pos->y = y;
    return TRUE;
}

/***********************************************************************
 *           HAIKU_SetCursorPos
 */
BOOL HAIKU_SetCursorPos( INT x, INT y )
{
    haiku_set_mouse( x, y );
    return TRUE;
}
