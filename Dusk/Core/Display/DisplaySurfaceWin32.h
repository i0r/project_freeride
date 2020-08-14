/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_WINAPI
struct NativeDisplaySurface
{
    HINSTANCE	Instance;
    HWND		Handle;
    LPCWSTR		ClassName;
    HDC			DrawContext;
    u32         WindowWidth;
    u32         WindowHeight;
    u32         ClientWidth;
    u32         ClientHeight;

    struct {
        u32     ShouldQuit      : 1;
        u32     IsMouseVisible  : 1;
        u32     HasFocus        : 1;
        u32     HasLostFocus    : 1;
        u32     IsResizing : 1;
        u32     HasBeenResized : 1;
    } Flags;

    ~NativeDisplaySurface()
    {
        ShowWindow( Handle, SW_HIDE );
        ShowCursor( TRUE );
        CloseWindow( Handle );

        UnregisterClass( ClassName, Instance );

        DeleteObject( DrawContext );
        DeleteObject( Instance );
        DestroyWindow( Handle );

        ClientWidth = 0;
        ClientHeight = 0;
    }
};
#endif
