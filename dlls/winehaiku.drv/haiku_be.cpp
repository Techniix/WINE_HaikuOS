/*
 * Haiku driver: glue code for the Haiku (Be API) application, window and
 * view classes
 *
 * Copyright 2026 Techniix
 *
 * Based on the Wine 7.1 Haiku driver by X512.
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

/* Only Haiku headers here; everything that touches Wine is in the C files.
 * Nothing in this file may call into Wine, since it mostly runs on threads
 * created by the Haiku application kit. */

#include <Application.h>
#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <OS.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>
#include <WindowScreen.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <new>

#include "haiku_be.h"

static int event_fd = -1;

static void post_event( const struct haiku_event &event )
{
    ssize_t ret;

    /* the pipe is non-blocking: if Wine can't keep up we rather drop events
     * than block a window thread */
    do ret = write( event_fd, &event, sizeof(event) );
    while (ret < 0 && errno == EINTR);
}

static struct haiku_event make_event( int32_t type, uint64_t hwnd )
{
    struct haiku_event event;

    memset( &event, 0, sizeof(event) );
    event.type = type;
    event.hwnd = hwnd;
    return event;
}


class WineView : public BView
{
public:
    WineView( BRect frame, uint64_t hwnd )
        : BView( frame, "wine", B_FOLLOW_ALL, B_WILL_DRAW ),
          fHwnd( hwnd ), fBitmap( NULL ), fOrigin( 0, 0 ), fButtons( 0 )
    {
        /* we draw every pixel ourselves */
        SetViewColor( B_TRANSPARENT_COLOR );
    }

    ~WineView()
    {
        delete fBitmap;
    }

    void Draw( BRect update )
    {
        if (!fBitmap) return;
        BRect src = update.OffsetByCopy( -fOrigin.x, -fOrigin.y ) & fBitmap->Bounds();
        if (src.IsValid()) DrawBitmap( fBitmap, src, src.OffsetByCopy( fOrigin ) );
    }

    /* called with the window locked */
    void UpdateBits( const void *bits, int stride, int width, int height, BPoint origin, BRect dirty )
    {
        if (origin != fOrigin)
        {
            fOrigin = origin;
            Invalidate();
        }
        if (!fBitmap || fBitmap->Bounds().IntegerWidth() + 1 != width ||
            fBitmap->Bounds().IntegerHeight() + 1 != height)
        {
            delete fBitmap;
            fBitmap = new (std::nothrow) BBitmap( BRect( 0, 0, width - 1, height - 1 ), B_RGB32 );
            if (fBitmap && fBitmap->InitCheck() != B_OK)
            {
                delete fBitmap;
                fBitmap = NULL;
            }
            if (!fBitmap) return;
            dirty = fBitmap->Bounds();
        }

        int top = (int)dirty.top, bottom = (int)dirty.bottom;
        int left = (int)dirty.left, right = (int)dirty.right;
        int32 dst_stride = fBitmap->BytesPerRow();
        const uint8 *src = (const uint8 *)bits;
        uint8 *dst = (uint8 *)fBitmap->Bits();

        /* both are 32-bit BGRX, top-down */
        for (int y = top; y <= bottom; y++)
            memcpy( dst + y * dst_stride + left * 4, src + y * stride + left * 4, (right - left + 1) * 4 );

        Invalidate( dirty.OffsetByCopy( fOrigin ) );
    }

    void MouseDown( BPoint where )
    {
        /* keep getting events while a button is held, also outside the window */
        SetMouseEventMask( B_POINTER_EVENTS, B_NO_POINTER_HISTORY );
        SendMouse( where );
    }

    void MouseUp( BPoint where )
    {
        SendMouse( where );
    }

    void MouseMoved( BPoint where, uint32 transit, const BMessage *drag )
    {
        SendMouse( where );
    }

    void MessageReceived( BMessage *msg )
    {
        if (msg->what == B_MOUSE_WHEEL_CHANGED)
        {
            struct haiku_event event = make_event( HAIKU_EVENT_WHEEL, fHwnd );
            if (msg->FindFloat( "be:wheel_delta_x", &event.wheel_x ) != B_OK) event.wheel_x = 0;
            if (msg->FindFloat( "be:wheel_delta_y", &event.wheel_y ) != B_OK) event.wheel_y = 0;
            post_event( event );
            return;
        }
        BView::MessageReceived( msg );
    }

private:
    void SendMouse( BPoint where )
    {
        struct haiku_event event = make_event( HAIKU_EVENT_MOUSE, fHwnd );
        BMessage *msg = Window()->CurrentMessage();
        int32 buttons = 0;

        if (msg && msg->FindInt32( "buttons", &buttons ) != B_OK) buttons = 0;
        else if (!msg) buttons = fButtons;

        ConvertToScreen( &where );
        event.x = (int32_t)where.x;
        event.y = (int32_t)where.y;
        event.buttons = buttons;
        event.old_buttons = fButtons;
        fButtons = buttons;
        post_event( event );
    }

    uint64_t fHwnd;
    BBitmap *fBitmap;
    BPoint   fOrigin;
    uint32   fButtons;
};


class WineWindow : public BWindow
{
public:
    WineWindow( uint64_t hwnd, bool popup )
        /* Wine draws the non-client area itself, so no Haiku decorations */
        : BWindow( BRect( 0, 0, 0, 0 ), "Wine", B_NO_BORDER_WINDOW_LOOK,
                   popup ? B_FLOATING_APP_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL,
                   B_ASYNCHRONOUS_CONTROLS | B_NOT_RESIZABLE | B_NOT_ZOOMABLE |
                   (popup ? B_AVOID_FOCUS : 0) ),
          fHwnd( hwnd )
    {
        fView = new WineView( Bounds(), hwnd );
        AddChild( fView );
        fView->MakeFocus();
    }

    bool QuitRequested()
    {
        /* let Wine decide, it will destroy the window if needed */
        post_event( make_event( HAIKU_EVENT_CLOSE, fHwnd ) );
        return false;
    }

    void WindowActivated( bool active )
    {
        struct haiku_event event = make_event( HAIKU_EVENT_ACTIVATE, fHwnd );
        event.active = active;
        post_event( event );
    }

    void ScreenChanged( BRect frame, color_space mode )
    {
        post_event( make_event( HAIKU_EVENT_SCREEN_CHANGED, fHwnd ) );
    }

    void DispatchMessage( BMessage *msg, BHandler *target )
    {
        /* Pass all keys to Wine, bypassing the window's keyboard shortcuts
         * (Alt+Q/W/X/C/V/A with the default keymap). */
        switch (msg->what)
        {
        case B_KEY_DOWN:
        case B_UNMAPPED_KEY_DOWN:
        case B_KEY_UP:
        case B_UNMAPPED_KEY_UP:
        {
            int32 key;
            bool down = msg->what == B_KEY_DOWN || msg->what == B_UNMAPPED_KEY_DOWN;
            struct haiku_event event = make_event( down ? HAIKU_EVENT_KEY_DOWN : HAIKU_EVENT_KEY_UP, fHwnd );

            if (msg->FindInt32( "key", &key ) == B_OK)
            {
                event.key = key;
                post_event( event );
            }
            return;
        }
        }
        BWindow::DispatchMessage( msg, target );
    }

    WineView *fView;

private:
    uint64_t fHwnd;
};


struct haiku_window : public WineWindow
{
    haiku_window( uint64_t hwnd, bool popup ) : WineWindow( hwnd, popup ) {}
};


class WineApplication : public BApplication
{
public:
    WineApplication() : BApplication( "application/x-vnd.Wine" ) {}

    void DispatchMessage( BMessage *msg, BHandler *target )
    {
        /* Wine rewrites the process arguments at startup, so the copy Haiku
         * sends us doesn't parse (and we don't need it anyway). */
        if (msg->what == B_ARGV_RECEIVED) return;
        BApplication::DispatchMessage( msg, target );
    }
};

static sem_id app_ready = -1;

static int32 app_thread( void *arg )
{
    new WineApplication();
    release_sem( app_ready );
    be_app->Run();
    return 0;
}

extern "C" int haiku_be_init( int fd )
{
    thread_id thread;

    event_fd = fd;
    if (be_app) return 0;  /* already initialized */

    if ((app_ready = create_sem( 0, "wine app ready" )) < B_OK) return -1;
    thread = spawn_thread( app_thread, "wine application", B_NORMAL_PRIORITY, NULL );
    if (thread < B_OK || resume_thread( thread ) != B_OK) return -1;
    acquire_sem( app_ready );
    delete_sem( app_ready );
    return 0;
}

extern "C" haiku_window *haiku_window_create( uint64_t hwnd, int popup )
{
    haiku_window *win = new (std::nothrow) haiku_window( hwnd, popup != 0 );

    if (!win) return NULL;
    win->Run();  /* starts the window thread; the window stays hidden */
    return win;
}

extern "C" void haiku_window_destroy( haiku_window *win )
{
    if (!win || !win->Lock()) return;
    win->Quit();  /* deletes the window */
}

extern "C" void haiku_window_set_frame( haiku_window *win, int x, int y, int width, int height )
{
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (!win->Lock()) return;
    win->MoveTo( x, y );
    win->ResizeTo( width - 1, height - 1 );
    win->Unlock();
}

extern "C" void haiku_window_show( haiku_window *win, int show, int activate )
{
    if (!win->Lock()) return;
    if (show)
    {
        while (win->IsHidden()) win->Show();
        if (activate) win->Activate( true );
    }
    else
    {
        while (!win->IsHidden()) win->Hide();
    }
    win->Unlock();
}

extern "C" void haiku_window_set_title( haiku_window *win, const char *utf8 )
{
    if (!win->Lock()) return;
    win->SetTitle( utf8 );
    win->Unlock();
}

extern "C" void haiku_window_update( haiku_window *win, const void *bits, int stride, int width, int height,
                                     int origin_x, int origin_y, int left, int top, int right, int bottom )
{
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > width) right = width;
    if (bottom > height) bottom = height;
    if (left >= right || top >= bottom) return;

    if (!win->Lock()) return;
    win->fView->UpdateBits( bits, stride, width, height, BPoint( origin_x, origin_y ),
                            BRect( left, top, right - 1, bottom - 1 ) );
    win->Unlock();
}

extern "C" void haiku_get_screen_info( int *width, int *height, int *bpp, int *refresh )
{
    BScreen screen( B_MAIN_SCREEN_ID );
    display_mode mode;
    BRect frame = screen.Frame();

    *width = frame.IntegerWidth() + 1;
    *height = frame.IntegerHeight() + 1;
    *bpp = 32;
    *refresh = 60;
    if (screen.GetMode( &mode ) == B_OK && mode.timing.h_total && mode.timing.v_total)
        *refresh = (int)((mode.timing.pixel_clock * 1000.0) /
                         ((double)mode.timing.h_total * mode.timing.v_total) + 0.5);
}

extern "C" int haiku_get_mouse( int *x, int *y )
{
    BPoint where;
    uint32 buttons;

    if (get_mouse( &where, &buttons ) != B_OK) return 0;
    *x = (int)where.x;
    *y = (int)where.y;
    return 1;
}

extern "C" void haiku_set_mouse( int x, int y )
{
    set_mouse_position( x, y );
}
