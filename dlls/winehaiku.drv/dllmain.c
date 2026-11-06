/*
 * Haiku driver PE side
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

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"

#include "unixlib.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);

/* Haiku's window threads can't call into Wine, so they hand their events to
 * this thread, which feeds them to win32u. */
static DWORD WINAPI haiku_events_thread( void *arg )
{
    WINE_UNIX_CALL( haikudrv_unix_func_read_events, NULL );
    ERR( "Haiku event loop failed, terminating process\n" );
    TerminateProcess( GetCurrentProcess(), 1 );
    return 0;
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    DWORD tid;

    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    DisableThreadLibraryCalls( instance );
    if (__wine_init_unix_call()) return FALSE;
    if (WINE_UNIX_CALL( haikudrv_unix_func_init, NULL )) return FALSE;

    CloseHandle( CreateThread( NULL, 0, haiku_events_thread, NULL, 0, &tid ));
    return TRUE;
}
