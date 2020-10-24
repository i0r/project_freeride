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

// RAII helper to enumerate the monitor list on the system.
struct MonitorEnumeratorWin32 
{
    // The list of monitors found on the system.
    // Note that you can use the operator overloads to avoid accessing the field explicitly.
    std::vector<RECT>   monitorsList;

    static BOOL CALLBACK EnumCallback( HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData )
    {
        MonitorEnumeratorWin32* thisPointer = reinterpret_cast< MonitorEnumeratorWin32* >( pData );
        thisPointer->monitorsList.push_back( *lprcMonitor );
        return TRUE;
    }

    MonitorEnumeratorWin32()
    {
        EnumDisplayMonitors( 0, 0, EnumCallback, reinterpret_cast<LPARAM>( this ) );
    }

    auto begin()
    {
        return monitorsList.begin();
    }

    auto end()
    {
        return monitorsList.end();
    }

    const RECT& operator [] ( const i32 index ) const
    {
        return monitorsList.at( index );
    }
};

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

	case WM_SIZE:
        nativeSurface->Flags.IsResizing = 1;
		break;

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

void DisplaySurface::create( const u32 surfaceWidth, const u32 surfaceHeight, const eDisplayMode initialDisplayMode, const i32 initialMonitorIndex )
{
    constexpr dkChar_t* className = DUSK_STRING( "Dusk GameEngine Window" );

    DUSK_LOG_INFO( "Creating display surface (WIN32)\n" );

    displayMode = initialDisplayMode;
    monitorIndex = initialMonitorIndex;

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

    DWORD windowExFlags = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, windowFlags = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

    MonitorEnumeratorWin32 monitorList;
    
    const RECT& monitorInfos = monitorList[monitorIndex];

    i32 windowOriginX = monitorInfos.left;
    i32 windowOriginY = monitorInfos.top;
    u32 screenWidth = static_cast< u32 >( monitorInfos.right - monitorInfos.left );
    u32 screenHeight = static_cast< u32 >( monitorInfos.bottom - monitorInfos.top );
    DUSK_LOG_INFO( "Selected monitor index %u (origin %ix%i dimension %ux%u)\n", monitorIndex, windowOriginX, windowOriginY, screenWidth, screenHeight );

    HWND handle = CreateWindowEx( windowExFlags, className, className, windowFlags, windowOriginX, windowOriginY, surfaceWidth, surfaceHeight, NULL, NULL, instance, NULL );
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
    originX = windowOriginX;
    originY = windowOriginY;

    SetWindowLongPtr( handle, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( displaySurface ) );
}

void DisplaySurface::setCaption( const dkChar_t* caption )
{
    SetWindowText( displaySurface->Handle, caption );
}

bool DisplaySurface::hasReceivedQuitSignal() const
{
    return displaySurface->Flags.ShouldQuit;
}

bool DisplaySurface::hasReceivedResizeEvent() const
{
    return displaySurface->Flags.HasBeenResized;
}

void DisplaySurface::acknowledgeResizeEvent()
{
    displaySurface->Flags.HasBeenResized = 0;
}

void DisplaySurface::pollSystemEvents( InputReader* inputReader )
{
    MSG msg = { 0 };

    bool isResizing = false;

    while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
        switch ( msg.message ) {
        case WM_INPUT:
            dk::input::ProcessInputEventImpl( inputReader, msg.hwnd, msg.lParam, msg.time );
            break;
        case WM_SIZE:
            displaySurface->Flags.IsResizing = 1;
            isResizing = true;
            break;
        case WM_CHAR:
            if ( msg.wParam > 0 && msg.wParam < 0x10000 ) {
                inputReader->pushKeyStroke( static_cast< dk::input::eInputKey >( msg.wParam ) );
            }
            break;
        };

        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    if ( !isResizing && displaySurface->Flags.IsResizing )  {
        displaySurface->Flags.IsResizing = false;
        displaySurface->Flags.HasBeenResized = true;

        RECT windowRectangle;
        GetWindowRect( displaySurface->Handle, &windowRectangle );

        DWORD clientWidth = windowRectangle.right - windowRectangle.left;
        DWORD clientHeight = windowRectangle.bottom - windowRectangle.top;

        displaySurface->ClientWidth = clientWidth;
        displaySurface->ClientHeight = clientHeight;

        width = displaySurface->ClientWidth;
        height = displaySurface->ClientHeight;
    }
}

void DisplaySurface::setWindowedDisplayMode()
{
    width = displaySurface->ClientWidth;
    height = displaySurface->ClientHeight;
    MoveWindow( displaySurface->Handle, originX, originY, width, height, TRUE );

    SetWindowLongPtr( displaySurface->Handle, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE );
    SetWindowLongPtr( displaySurface->Handle, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE );

    if ( displayMode == FULLSCREEN ) {
        SetWindowPos( displaySurface->Handle, 0, originX, originY, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW );
        ChangeDisplaySettings( 0, 0 );
    }
}

void DisplaySurface::setFullscreenDisplayMode()
{
    MonitorEnumeratorWin32 monitorList;
    const RECT& monitorInfos = monitorList[monitorIndex];

    u32 screenWidth = static_cast< u32 >( monitorInfos.right - monitorInfos.left );
    u32 screenHeight = static_cast< u32 >( monitorInfos.bottom - monitorInfos.top );

	DEVMODE devMode = {};
	devMode.dmSize = sizeof( devMode );
	devMode.dmPelsWidth = screenWidth;
	devMode.dmPelsHeight = screenHeight;
    devMode.dmBitsPerPel = 32;
	devMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    DISPLAY_DEVICE displayDeviceInfos;
    displayDeviceInfos.cb = sizeof( DISPLAY_DEVICE );
    BOOL hasRetrievedInfos = EnumDisplayDevicesW( nullptr, monitorIndex, &displayDeviceInfos, 0 );

    if ( hasRetrievedInfos ) {
        LONG changeDispResult = ChangeDisplaySettingsEx( displayDeviceInfos.DeviceName, &devMode, NULL, CDS_FULLSCREEN, NULL );
        if ( changeDispResult != DISP_CHANGE_SUCCESSFUL ) {
            DUSK_LOG_WARN( "Failed to toggle fullscreen (ChangeDisplaySettings returned 0x%x; %u x %u)\n", changeDispResult, screenWidth, screenHeight );
        } else {
            width = static_cast< u32 >( screenWidth );
            height = static_cast< u32 >( screenHeight );
        }
    } else {
        DUSK_LOG_WARN( "Failed to toggle fullscreen (EnumDisplayDevices failure)\n" );
    }
}

void DisplaySurface::setFullscreenBorderlessDisplayMode()
{
    MonitorEnumeratorWin32 monitorList;
    const RECT& monitorInfos = monitorList[monitorIndex];

    u32 newWidth = static_cast< u32 >( monitorInfos.right - monitorInfos.left );
    u32 newHeight = static_cast< u32 >( monitorInfos.bottom - monitorInfos.top );

	SetWindowLongPtr( displaySurface->Handle, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE );
	MoveWindow( displaySurface->Handle, monitorInfos.left, monitorInfos.top, newWidth, newHeight, TRUE );

    width = newWidth;
    height = newHeight;
}
#endif

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
