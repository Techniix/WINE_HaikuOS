/*
 * Haiku driver display devices
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

#include "haikudrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(haikudrv);

/***********************************************************************
 *           HAIKU_UpdateDisplayDevices
 *
 * Reports the main screen as the only monitor, with its current mode.
 * FIXME: multiple screens, mode changes.
 */
UINT HAIKU_UpdateDisplayDevices( const struct gdi_device_manager *device_manager, void *param )
{
    struct pci_id pci_id = {0};
    struct gdi_monitor monitor = {0};
    DEVMODEW mode = {.dmSize = sizeof(mode)};
    int width, height, bpp, refresh;

    haiku_get_screen_info( &width, &height, &bpp, &refresh );
    TRACE( "screen %dx%d %d bpp %d Hz\n", width, height, bpp, refresh );

    device_manager->add_gpu( "Haiku", &pci_id, NULL, param );
    device_manager->add_source( "Default", DISPLAY_DEVICE_ATTACHED_TO_DESKTOP | DISPLAY_DEVICE_PRIMARY_DEVICE,
                                NtUserGetSystemDpiForProcess( NULL ), param );

    SetRect( &monitor.rc_monitor, 0, 0, width, height );
    monitor.rc_work = monitor.rc_monitor;  /* FIXME: exclude the Deskbar */
    device_manager->add_monitor( &monitor, param );

    mode.dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                    DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY | DM_POSITION;
    mode.dmDisplayOrientation = DMDO_DEFAULT;
    mode.dmBitsPerPel = bpp;
    mode.dmPelsWidth = width;
    mode.dmPelsHeight = height;
    mode.dmDisplayFrequency = refresh;
    device_manager->add_modes( &mode, 1, &mode, param );

    return STATUS_SUCCESS;
}
