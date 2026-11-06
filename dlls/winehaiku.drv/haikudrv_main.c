/*
 * Haiku driver initialization and event handling
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "haikudrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);

static int event_pipe[2] = { -1, -1 };

static void send_mouse_buttons( HWND hwnd, const struct haiku_event *event )
{
    static const struct { UINT button; DWORD down, up; } buttons[] =
    {
        { HAIKU_BUTTON_PRIMARY,   MOUSEEVENTF_LEFTDOWN,   MOUSEEVENTF_LEFTUP },
        { HAIKU_BUTTON_SECONDARY, MOUSEEVENTF_RIGHTDOWN,  MOUSEEVENTF_RIGHTUP },
        { HAIKU_BUTTON_TERTIARY,  MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP },
    };
    INPUT input = {0};
    UINT changed = event->buttons ^ event->old_buttons, i;

    input.type = INPUT_MOUSE;
    input.mi.dx = event->x;
    input.mi.dy = event->y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    for (i = 0; i < ARRAY_SIZE(buttons); i++)
    {
        if (!(changed & buttons[i].button)) continue;
        input.mi.dwFlags |= (event->buttons & buttons[i].button) ? buttons[i].down : buttons[i].up;
    }
    NtUserSendHardwareInput( hwnd, 0, &input, 0 );
}

static void send_wheel( HWND hwnd, float delta, DWORD flags )
{
    INPUT input = {0};

    if (!delta) return;
    input.type = INPUT_MOUSE;
    /* Haiku reports positive values for scrolling down/right */
    input.mi.mouseData = (flags == MOUSEEVENTF_WHEEL ? -delta : delta) * WHEEL_DELTA;
    input.mi.dwFlags = flags;
    NtUserSendHardwareInput( hwnd, 0, &input, 0 );
}

static void send_key( HWND hwnd, int key, BOOL down )
{
    INPUT input = {0};
    UINT scan = haiku_key_to_scan( key );

    if (!(scan & 0xff)) return;
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = (scan & 0x300) ? scan + 0xdf00 : scan;
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (scan & ~0xff) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!down) input.ki.dwFlags |= KEYEVENTF_KEYUP;
    NtUserSendHardwareInput( hwnd, 0, &input, 0 );
}

void haiku_handle_event( const struct haiku_event *event )
{
    HWND hwnd = (HWND)(UINT_PTR)event->hwnd;

    TRACE( "type %d hwnd %p\n", event->type, hwnd );

    switch (event->type)
    {
    case HAIKU_EVENT_MOUSE:
        send_mouse_buttons( hwnd, event );
        break;
    case HAIKU_EVENT_WHEEL:
        send_wheel( hwnd, event->wheel_y, MOUSEEVENTF_WHEEL );
        send_wheel( hwnd, event->wheel_x, MOUSEEVENTF_HWHEEL );
        break;
    case HAIKU_EVENT_KEY_DOWN:
    case HAIKU_EVENT_KEY_UP:
        send_key( hwnd, event->key, event->type == HAIKU_EVENT_KEY_DOWN );
        break;
    case HAIKU_EVENT_CLOSE:
        NtUserPostMessage( hwnd, WM_SYSCOMMAND, SC_CLOSE, 0 );
        break;
    case HAIKU_EVENT_ACTIVATE:
        if (event->active) NtUserPostMessage( hwnd, WM_HAIKU_SET_FOREGROUND, 0, 0 );
        break;
    case HAIKU_EVENT_SCREEN_CHANGED:
        NtUserPostMessage( NtUserGetDesktopWindow(), WM_HAIKU_DISPLAY_CHANGED, 0, 0 );
        break;
    default:
        FIXME( "unknown event %d\n", event->type );
        break;
    }
}

static const struct user_driver_funcs haikudrv_funcs =
{
    .pCreateWindowSurface = HAIKU_CreateWindowSurface,
    .pDesktopWindowProc = HAIKU_DesktopWindowProc,
    .pDestroyWindow = HAIKU_DestroyWindow,
    .pGetCursorPos = HAIKU_GetCursorPos,
    .pSetCursorPos = HAIKU_SetCursorPos,
    .pSetWindowText = HAIKU_SetWindowText,
    .pUpdateDisplayDevices = HAIKU_UpdateDisplayDevices,
    .pWindowMessage = HAIKU_WindowMessage,
    .pWindowPosChanged = HAIKU_WindowPosChanged,
    .pWindowPosChanging = HAIKU_WindowPosChanging,
};

static NTSTATUS haikudrv_unix_init( void *arg )
{
    if (pipe2( event_pipe, O_CLOEXEC ))
    {
        ERR( "failed to create event pipe: %s\n", strerror( errno ));
        return STATUS_UNSUCCESSFUL;
    }
    fcntl( event_pipe[1], F_SETFL, O_NONBLOCK );

    if (haiku_be_init( event_pipe[1] ))
    {
        ERR( "failed to initialize the Haiku application\n" );
        return STATUS_UNSUCCESSFUL;
    }

    __wine_set_user_driver( &haikudrv_funcs, WINE_GDI_DRIVER_VERSION );
    return STATUS_SUCCESS;
}

static NTSTATUS haikudrv_unix_read_events( void *arg )
{
    struct haiku_event event;
    ssize_t ret;

    for (;;)
    {
        ret = read( event_pipe[0], &event, sizeof(event) );
        if (ret == sizeof(event)) haiku_handle_event( &event );
        else if (ret < 0 && errno == EINTR) continue;
        else break;
    }
    ERR( "reading Haiku events failed: %s\n", ret < 0 ? strerror( errno ) : "short read" );
    return STATUS_UNSUCCESSFUL;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    haikudrv_unix_init,
    haikudrv_unix_read_events,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_funcs) == haikudrv_unix_func_count );

#ifdef _WIN64

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    haikudrv_unix_init,
    haikudrv_unix_read_events,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_wow64_funcs) == haikudrv_unix_func_count );

#endif /* _WIN64 */
