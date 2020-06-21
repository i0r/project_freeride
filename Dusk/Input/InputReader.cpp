/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "InputReader.h"

#include "InputMapper.h"
#include "InputAxis.h"

#if DUSK_WIN
#include "InputReaderWin32.h"
#elif DUSK_UNIX
#include "InputReaderXcb.h"
#endif

#include <string.h>

using namespace dk::input;

InputReader::InputReader()
    : activeInputLayout( INPUT_LAYOUT_QWERTY )
    , textInput( 1024 )
    , absoluteAxisValues{ 0.0 }
    , buttonMap{ false }
{

}

InputReader::~InputReader()
{
    memset( buttonMap, 0, sizeof( bool ) * KEYBOARD_KEY_COUNT );
}

void InputReader::create()
{
    CreateInputReaderImpl( activeInputLayout );
}

void InputReader::onFrame( InputMapper* inputMapper )
{
    // Process and dispatch captured events to an input mapper
    while ( !inputKeyEventQueue.empty() ) {
        auto& inputKeyEvent = inputKeyEventQueue.front();
        auto wasPreviouslyDown = buttonMap[inputKeyEvent.InputKey];

        inputMapper->setRawButtonState( inputKeyEvent.InputKey, inputKeyEvent.IsDown, wasPreviouslyDown );

        // Update Previous Key State
        buttonMap[inputKeyEvent.InputKey] = inputKeyEvent.IsDown;

        inputKeyEventQueue.pop();
    }

    // Do the same for axis updates
    while ( !inputAxisEventQueue.empty() ) {
        auto& inputAxisEvent = inputAxisEventQueue.front();

        inputMapper->setRawAxisValue( inputAxisEvent.InputAxis, inputAxisEvent.RawAxisValue );

        inputAxisEventQueue.pop();
    }
}

dk::input::eInputLayout InputReader::getActiveInputLayout() const
{
    return activeInputLayout;
}

void InputReader::pushKeyEvent( InputReader::KeyEvent&& keyEvent )
{
    inputKeyEventQueue.push( std::move( keyEvent ) );
}

void InputReader::pushAxisEvent( InputReader::AxisEvent&& axisEvent )
{
    inputAxisEventQueue.push( std::move( axisEvent ) );
}

void InputReader::pushKeyStroke( const eInputKey keyStroke )
{
    textInput.push_back( keyStroke );
}

std::vector<dk::input::eInputKey> InputReader::getAndFlushKeyStrokes()
{
    auto keyStrokes = textInput;
    textInput.clear();
    return keyStrokes;
}

void InputReader::setAbsoluteAxisValue( const dk::input::eInputAxis axis, const double absoluteValue )
{
    absoluteAxisValues[axis] = absoluteValue;
}

double InputReader::getAbsoluteAxisValue( const dk::input::eInputAxis axis )
{
    return absoluteAxisValues[axis];
}
