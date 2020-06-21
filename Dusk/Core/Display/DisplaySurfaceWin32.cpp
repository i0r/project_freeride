/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_WINAPI
#include "DisplaySurfaceWin32.h"
#include "DisplaySurface.h"

#include "Input/InputReader.h"
#include "Input/InputReaderWin32.h"

LRESULT CALLBACK WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    NativeDisplaySurface* nativeSurface = reinterpret_cast<NativeDisplaySurface*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );

    if ( nativeSurface == nullptr ) {
        return DefWindowProc( hwnd, uMsg, wParam, lParam );
    }

    switch ( uMsg ) {
    case WM_DESTROY:
        DestroyWindow( hwnd );
        return 0;

    case WM_CLOSE:
        nativeSurface->Flags.ShouldQuit = 1;
        PostQuitMessage( 0 );
        return 0;

    case WM_SETCURSOR: {
        WORD ht = LOWORD( lParam );

        // Detect if the mouse is inside the windows rectangle
        // If it is not (e.g. the mouse is on the window title bar), display the cursor
        if ( HTCLIENT == ht && nativeSurface->Flags.IsMouseVisible ) {
            nativeSurface->Flags.IsMouseVisible = 0;
            ShowCursor( FALSE );
        } else if ( HTCLIENT != ht && nativeSurface->Flags.IsMouseVisible == 0 ) {
            nativeSurface->Flags.IsMouseVisible = 1;
            ShowCursor( TRUE );
        }
    } return 0;

    case WM_ACTIVATE:
    case WM_SETFOCUS:
        nativeSurface->Flags.HasFocus = 1;
        return 0;

    case WA_INACTIVE:
    case WM_KILLFOCUS:
        nativeSurface->Flags.HasFocus = 0;
        nativeSurface->Flags.HasLostFocus = 1;
        return 0;

    default:
        break;
    }

    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

DisplaySurface::~DisplaySurface()
{
    dk::core::free( memoryAllocator, displaySurface );

    memoryAllocator = nullptr;
    displaySurface = nullptr;
}

void DisplaySurface::create( const u32 surfaceWidth, const u32 surfaceHeight, const eDisplayMode initialDisplayMode )
{
    constexpr dkChar_t* className = DUSK_STRING( "Dusk GameEngine Window" );

    DUSK_LOG_INFO( "Creating display surface (WIN32)\n" );

    displayMode = initialDisplayMode;

    HMODULE instance = GetModuleHandle( NULL );
    HICON hIcon = LoadIcon( instance, MAKEINTRESOURCE( 101 ) );

    const WNDCLASSEX wc = {
        sizeof( WNDCLASSEX ),									// cbSize
        CS_HREDRAW | CS_VREDRAW | CS_OWNDC,						// style
        ::WndProc,												// lpdkWndProc
        NULL,													// cbClsExtra
        NULL,													// cbWndExtra
        instance,											    // hInstance
        hIcon,			                                        // hIcon
        LoadCursor( 0, IDC_ARROW ),								// hCursor
        static_cast< HBRUSH >( GetStockObject( BLACK_BRUSH ) ),	// hbrBackground
        NULL,													// lpszMenuName
        className,											    // lpszClassName
        NULL 													// hIconSm
    };

    if ( RegisterClassEx( &wc ) == FALSE ) {
        return;
    }

    DWORD windowExFlags = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, windowFlags = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | ( WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_VISIBLE );

    i32 screenWidth = GetSystemMetrics( SM_CXSCREEN ),
        screenHeight = GetSystemMetrics( SM_CYSCREEN );

    HWND handle = CreateWindowEx( windowExFlags, className, className, windowFlags, ( screenWidth - surfaceWidth ) / 2, ( screenHeight - surfaceHeight ) / 2, surfaceWidth, surfaceHeight, NULL, NULL, instance, NULL );

    if ( handle == nullptr ) {
        return;
    }

    SendMessage( handle, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon );
    SendMessage( handle, WM_SETICON, ICON_BIG, ( LPARAM )hIcon );

    HDC drawContext = GetDC( handle );

    ShowWindow( handle, SW_SHOWNORMAL );

    UpdateWindow( handle );

    SetForegroundWindow( handle );
    SetFocus( handle );

    RECT rcClient, rcWind;
    POINT ptDiff;
    GetClientRect( handle, &rcClient );
    GetWindowRect( handle, &rcWind );
    ptDiff.x = ( rcWind.right - rcWind.left ) - rcClient.right;
    ptDiff.y = ( rcWind.bottom - rcWind.top ) - rcClient.bottom;

    i32 clientWidth = static_cast< i32 >( surfaceWidth + ptDiff.x );
    i32 clientHeight = static_cast< i32 >( surfaceHeight + ptDiff.y );

    MoveWindow( handle, rcWind.left - ptDiff.x, rcWind.top, clientWidth, clientHeight, TRUE );

    displaySurface = dk::core::allocate<NativeDisplaySurface>( memoryAllocator );
    displaySurface->ClassName = className;
    displaySurface->DrawContext = drawContext;
    displaySurface->Handle = handle;
    displaySurface->Instance = instance;
    displaySurface->WindowWidth = surfaceWidth;
    displaySurface->WindowHeight = surfaceHeight;
    displaySurface->ClientWidth = clientWidth;
    displaySurface->ClientHeight = clientHeight;

    width = clientWidth;
    height = clientHeight;

    SetWindowLongPtr( handle, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( displaySurface ) );
}

void DisplaySurface::setCaption( const dkChar_t* caption )
{
    SetWindowText( displaySurface->Handle, caption );
}

bool DisplaySurface::hasReceivedQuitSignal() const
{
    return ( displaySurface->Flags.ShouldQuit & 1 );
}

void DisplaySurface::pollSystemEvents( InputReader* inputReader )
{
    MSG msg = { 0 };

    while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
        if ( msg.message == WM_INPUT ) {
            dk::input::ProcessInputEventImpl( inputReader, msg.hwnd, msg.lParam, msg.time );
        } else if ( msg.message == WM_QUIT ) {
            displaySurface->Flags.ShouldQuit = 1;
        } else if ( msg.message == WM_CHAR ) {
            if ( msg.wParam > 0 && msg.wParam < 0x10000 ) {
                inputReader->pushKeyStroke( static_cast< dk::input::eInputKey >( msg.wParam ) );
            }
        }

        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }
}
#endif
//
//#if DUSK_WIN
//#include "DisplaySurface.h"
//#include "EventLoopWin32.h"
//
//void dk::display::ToggleFullscreenImpl( NativeDisplaySurface* surface )
//{
//    DWORD windowExFlags = WS_EX_APPWINDOW, windowFlags = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
//
//    i32 screenWidth = GetSystemMetrics( SM_CXSCREEN ),
//        screenHeight = GetSystemMetrics( SM_CYSCREEN );
//
//    RECT windowRectangle;
//    GetWindowRect( surface->Handle, &windowRectangle );
//
//    long width = windowRectangle.right - windowRectangle.left;
//    long height = windowRectangle.bottom - windowRectangle.top;
//
//    DEVMODE devMode = {};
//    devMode.dmSize = sizeof( devMode );
//    devMode.dmPelsWidth = width;
//    devMode.dmPelsHeight = height;
//    devMode.dmBitsPerPel = 32;
//    devMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
//
//    LONG changeDispResult = ChangeDisplaySettings( &devMode, CDS_FULLSCREEN );
//    if ( changeDispResult != DISP_CHANGE_SUCCESSFUL ) {
//        DUSK_LOG_WARN( "Failed to toggle fullscreen (ChangeDisplaySettings returned 0x%x)\n", changeDispResult );
//    }
//}
//
//void dk::display::ToggleBorderlessImpl( NativeDisplaySurface* surface )
//{
//    RECT windowRectangle;
//    GetWindowRect( surface->Handle, &windowRectangle );
//
//    auto width = windowRectangle.right - windowRectangle.left;
//    auto height = windowRectangle.bottom - windowRectangle.top;
//
//    SetWindowLongPtr( surface->Handle, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE );
//    MoveWindow( surface->Handle, 0, 0, width, height, TRUE );
//}
//
//void dk::display::SetMousePositionImpl( NativeDisplaySurface* surface, const f32 surfaceNormalizedX, const f32 surfaceNormalizedY )
//{
//    // Don't trap mouse if the user has alt-tabbed the application
//    if ( surface->Flags.HasFocus == 0 ) {
//        return;
//    }
//
//    POINT screenPoint = { 0 };
//    screenPoint.x = static_cast< LONG >( surface->ClientWidth * surfaceNormalizedX );
//    screenPoint.y = static_cast< LONG >( surface->ClientHeight * surfaceNormalizedY );
//
//    ClientToScreen( surface->Handle, &screenPoint );
//
//    SetCursorPos( screenPoint.x, screenPoint.y );
//}
//#endif
