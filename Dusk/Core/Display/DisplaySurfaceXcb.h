/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_XCB
#include <xcb/xcb.h>
#include <X11/Xlib.h>

struct NativeDisplaySurface
{
    xcb_connection_t*   Connection;
    xcb_window_t      WindowInstance;

    xcb_atom_t WindowProtocolAtom;
    xcb_atom_t DeleteAtom;

    u32         WindowWidth;
    u32         WindowHeight;

    Cursor      BlankCursor;
    Display*    XorgDisplay;

    struct {
        u32     ShouldQuit      : 1;
        u32     IsMouseVisible  : 1;
        u32     HasFocus        : 1;
        u32     HasLostFocus    : 1;
        u32     IsResizing      : 1;
    } Flags;

    ~NativeDisplaySurface()
    {
        xcb_destroy_window( Connection, WindowInstance );

        WindowWidth = 0;
        WindowHeight = 0;
    }
};
#endif
