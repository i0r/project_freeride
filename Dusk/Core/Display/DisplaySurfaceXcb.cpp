/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_XCB
#include "DisplaySurfaceXcb.h"
#include "DisplaySurface.h"

#include <Input/InputReaderXcb.h>
#include <string.h>

inline xcb_intern_atom_cookie_t GetCookieForAtom( xcb_connection_t* connection, const dkString_t& stateName )
{
    return xcb_intern_atom( connection, 0, static_cast<uint16_t>( stateName.size() ), stateName.c_str() );
}

xcb_atom_t GetReplyAtomFromCookie( xcb_connection_t* connection, xcb_intern_atom_cookie_t cookie )
{
    xcb_generic_error_t* error = nullptr;
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply( connection, cookie, &error );
    DUSK_RAISE_FATAL_ERROR( error == nullptr, "Internal failure! (error code: 0x%x)\n", error->error_code );

    return reply->atom;
}

DisplaySurface::~DisplaySurface()
{
    dk::core::free( memoryAllocator, displaySurface );

    memoryAllocator = nullptr;
    displaySurface = nullptr;
}

void DisplaySurface::create( const u32 surfaceWidth, const u32 surfaceHeight, const eDisplayMode initialDisplayMode, const i32 initialMonitorIndex )
{
    DUSK_LOG_INFO( "Creating display surface (XCB)\n" );

    displayMode = initialDisplayMode;
    xcb_connection_t* connection = xcb_connect( nullptr, nullptr );
    int error = xcb_connection_has_error(connection);
    DUSK_RAISE_FATAL_ERROR( ( connection != nullptr ) || error,"Failed to connect to Xorg server (error code: %i)", error );

    DUSK_LOG_INFO(  "Connected to X11 server!\n" );

    xcb_screen_t* screen = xcb_setup_roots_iterator( xcb_get_setup( connection ) ).data;

    // Create native resource object
    displaySurface = dk::core::allocate<NativeDisplaySurface>( memoryAllocator );

    displaySurface->Connection = connection;
    displaySurface->WindowInstance = xcb_generate_id( displaySurface->Connection );

    displaySurface->WindowWidth = surfaceWidth;
    displaySurface->WindowHeight = surfaceHeight;

    constexpr u32 mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    u32 values[2] = {
        screen->black_pixel,
        ( XCB_EVENT_MASK_KEY_PRESS |
          XCB_EVENT_MASK_KEY_RELEASE |
          XCB_EVENT_MASK_EXPOSURE |
          XCB_EVENT_MASK_STRUCTURE_NOTIFY |
          XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
          XCB_EVENT_MASK_POINTER_MOTION |
          XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_BUTTON_RELEASE |
          XCB_EVENT_MASK_RESIZE_REDIRECT ),
    };

    xcb_create_window( displaySurface->Connection,
                       XCB_COPY_FROM_PARENT,
                       displaySurface->WindowInstance,
                       screen->root,
                       static_cast<int16_t>( surfaceWidth * 0.5f ),
                       static_cast<int16_t>( surfaceHeight * 0.5f ),
                       static_cast<uint16_t>( surfaceWidth ),
                       static_cast<uint16_t>( surfaceHeight ),
                       10,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                       screen->root_visual,
                       mask,
                       values
    );

    // Map the window on the screen
    xcb_map_window( displaySurface->Connection, displaySurface->WindowInstance );
    xcb_flush( displaySurface->Connection );

    DUSK_LOG_INFO( "Created and mapped window to screen successfully!\n" );

    // Register WM_DELETE atom
    displaySurface->WindowProtocolAtom = GetReplyAtomFromCookie( displaySurface->Connection, GetCookieForAtom( displaySurface->Connection, "WM_PROTOCOLS" ) );
    displaySurface->DeleteAtom  = GetReplyAtomFromCookie( displaySurface->Connection, GetCookieForAtom( displaySurface->Connection, "WM_DELETE_WINDOW" ) );

    xcb_change_property( displaySurface->Connection, XCB_PROP_MODE_REPLACE, displaySurface->WindowInstance, displaySurface->WindowProtocolAtom, XCB_ATOM_ATOM, 32, 1, &displaySurface->DeleteAtom );

    static constexpr const char* DEFAULT_WINDOW_NAME = "Dusk GameEngine Window";
    xcb_change_property( displaySurface->Connection, XCB_PROP_MODE_REPLACE, displaySurface->WindowInstance, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen( DEFAULT_WINDOW_NAME ), DEFAULT_WINDOW_NAME );

#if 0
    // Set default default icon
    i32 w, h, comp;
    auto image = stbi_load( "dusk_icon.png", &w, &h, &comp, STBI_rgb_alpha );
    if ( image != nullptr ) {
        const u32 texelCount = static_cast<u32>( w * h );
        const u32 iconBufferLength = texelCount + 2u; // First two integers contain icon width and height

        std::vector<u32> iconBuffer( iconBufferLength );
        iconBuffer[0] = static_cast<u32>( w );
        iconBuffer[1] = static_cast<u32>( h );

        memcpy( &iconBuffer[2], image, texelCount * static_cast<u32>( comp ) * sizeof( unsigned char ) );

        stbi_image_free( image );

        auto winIconAtom = GetReplyAtomFromCookie( surf->Connection, GetCookieForAtom( surf->Connection, "_NET_WM_ICON" ) );

        xcb_change_property( surf->Connection,
                             XCB_PROP_MODE_REPLACE,
                             surf->WindowInstance,
                             winIconAtom,
                             XCB_ATOM_CARDINAL,
                             32,
                             iconBufferLength,
                             iconBuffer.data() );
    }
#endif

    // Create blank cursor
    XColor dummy = {};
    char BLANK_CURSOR_TEXELS[1] = { 0 };

    Display* tmpDisp = XOpenDisplay( nullptr );

    Pixmap blank = XCreateBitmapFromData( tmpDisp, displaySurface->WindowInstance, BLANK_CURSOR_TEXELS, 1, 1 );

    auto blankCursor = XCreatePixmapCursor( tmpDisp, blank, blank, &dummy, &dummy, 0, 0 ) ;
    XFreePixmap( tmpDisp, blank );

    XDefineCursor( tmpDisp, displaySurface->WindowInstance, blankCursor );

    displaySurface->BlankCursor = blankCursor;
    displaySurface->XorgDisplay = tmpDisp;
    displaySurface->WindowWidth = surfaceWidth;
    displaySurface->WindowHeight = surfaceHeight;

    width = surfaceWidth;
    height = surfaceHeight;
}

void DisplaySurface::setCaption( const dkChar_t* caption )
{
    xcb_change_property( displaySurface->Connection, XCB_PROP_MODE_REPLACE, displaySurface->WindowInstance, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, static_cast<uint32_t>( strlen( caption ) ), caption );
}

bool DisplaySurface::hasReceivedQuitSignal() const
{
    return ( displaySurface->Flags.ShouldQuit & 1 );
}

void DisplaySurface::pollSystemEvents( InputReader* inputReader )
{
    xcb_generic_event_t* event = xcb_poll_for_event( displaySurface->Connection );

    while ( event != nullptr ) {
        switch ( event->response_type & 0x7F ) {
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t* clientMessage = reinterpret_cast<xcb_client_message_event_t *>( event );

            if( clientMessage->window != displaySurface->WindowInstance ) {
                break;
            }

            if( clientMessage->type == displaySurface->WindowProtocolAtom && clientMessage->data.data32[0] == displaySurface->DeleteAtom ) {
                displaySurface->Flags.ShouldQuit = 1;
            }
            break;
        }

        case XCB_EXPOSE: {
            xcb_client_message_event_t clientMessage;
            clientMessage.response_type = XCB_CLIENT_MESSAGE;
            clientMessage.format = 32;
            clientMessage.window = displaySurface->WindowInstance;
            clientMessage.type = XCB_ATOM_NOTICE;

            xcb_send_event( displaySurface->Connection, 0, displaySurface->WindowInstance, 0, reinterpret_cast< const char * >( &clientMessage ) );
            break;
        }

        case XCB_KEY_RELEASE:
        case XCB_KEY_PRESS:
        case XCB_BUTTON_RELEASE:
        case XCB_BUTTON_PRESS:
        case XCB_MOTION_NOTIFY:
            dk::input::ProcessInputEventImpl( inputReader, event );
            break;

        case XCB_ENTER_NOTIFY:
            XDefineCursor( displaySurface->XorgDisplay, displaySurface->WindowInstance, displaySurface->BlankCursor );
            break;

        case XCB_LEAVE_NOTIFY:
            XUndefineCursor( displaySurface->XorgDisplay, displaySurface->WindowInstance );
            break;

        default:
            break;
        }

        xcb_flush( displaySurface->Connection );
        free( event );

        event = xcb_poll_for_event( displaySurface->Connection );
    }
}

bool DisplaySurface::hasReceivedResizeEvent() const
{
    return false;
}
#endif
