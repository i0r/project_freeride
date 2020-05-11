/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_UNIX
#include "InputReaderXcb.h"

#include "InputReader.h"
#include "InputAxis.h"

#include <xcb/xcb_keysyms.h>

static xcb_key_symbols_t* KEY_SYMBOLS = nullptr;

static double previousX = 0.0;
static double previousY = 0.0;

using namespace dk::input;

void dk::input::CreateInputReaderImpl( dk::input::eInputLayout& activeInputLayout )
{
    DUSK_LOG_INFO( "Creating InputReader (XCB)\n" );

    xcb_connection_t* connection = xcb_connect( nullptr, nullptr );
    KEY_SYMBOLS = xcb_key_symbols_alloc( connection );
}

void dk::input::ProcessInputEventImpl( InputReader* inputReader, const xcb_generic_event_t* event )
{
    const auto responseType = ( event->response_type & 0x7F );

    switch( responseType ) {
    case XCB_MOTION_NOTIFY: {
        const xcb_motion_notify_event_t* motionNotifyEvent = reinterpret_cast<const xcb_motion_notify_event_t*>( event );

        inputReader->pushAxisEvent( { dk::input::eInputAxis::MOUSE_X, 0, static_cast<double>( motionNotifyEvent->event_x - previousX ) } );
        inputReader->pushAxisEvent( { dk::input::eInputAxis::MOUSE_Y, 0, static_cast<double>( motionNotifyEvent->event_y - previousY ) } );

        inputReader->setAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_X, motionNotifyEvent->event_x );
        inputReader->setAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_Y, motionNotifyEvent->event_y );

        previousX = motionNotifyEvent->event_x;
        previousY = motionNotifyEvent->event_y;

        break;
    }

    case XCB_KEY_RELEASE:{
        const xcb_key_release_event_t* keyReleaseEvent = reinterpret_cast<const xcb_key_release_event_t*>( event );
        inputReader->pushKeyEvent( { dk::input::UnixKeys[xcb_key_symbols_get_keysym( KEY_SYMBOLS, keyReleaseEvent->detail, 0 )], 0 } );
        break;
    }

    case XCB_KEY_PRESS: {
        const xcb_key_press_event_t* keyPressEvent = reinterpret_cast<const xcb_key_press_event_t*>( event );
        inputReader->pushKeyEvent( { dk::input::UnixKeys[xcb_key_symbols_get_keysym( KEY_SYMBOLS, keyPressEvent->detail, 0 )], 1 } );
        break;
    }

    case XCB_BUTTON_PRESS: {
        const xcb_button_press_event_t* pressEvent = reinterpret_cast<const xcb_button_press_event_t*>( event );

        switch ( pressEvent->detail ) {
        case XCB_BUTTON_INDEX_1:
            inputReader->pushKeyEvent( { dk::input::eInputKey::MOUSE_LEFT_BUTTON, true } );
            break;
        case XCB_BUTTON_INDEX_2:
            inputReader->pushKeyEvent( { dk::input::eInputKey::MOUSE_RIGHT_BUTTON, true } );
            break;
        case XCB_BUTTON_INDEX_3:
            inputReader->pushKeyEvent( { dk::input::eInputKey::MOUSE_MIDDLE_BUTTON, true } );
            break;
        default:
            break;
        }
        break;
    }

    case XCB_BUTTON_RELEASE: {
        const xcb_button_release_event_t* releaseEvent = reinterpret_cast<const xcb_button_release_event_t*>( event );

        switch ( releaseEvent->detail ) {
        case XCB_BUTTON_INDEX_1:
            inputReader->pushKeyEvent( { dk::input::eInputKey::MOUSE_LEFT_BUTTON, false } );
            break;
        case XCB_BUTTON_INDEX_2:
            inputReader->pushKeyEvent( { dk::input::eInputKey::MOUSE_RIGHT_BUTTON, false } );
            break;
        case XCB_BUTTON_INDEX_3:
            inputReader->pushKeyEvent( { dk::input::eInputKey::MOUSE_MIDDLE_BUTTON, false } );
            break;
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}
#endif
