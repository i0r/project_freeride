/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "InputLayouts.h"
#include "InputContext.h"
#include "MappedInput.h"
#include "InputAxis.h"
#include "InputKeys.h"

#include <queue>

class InputMapper;
struct NativeWindow;

using fnKeyStrokes_t = std::vector<dk::input::eInputKey>;

class InputReader
{
public:
    struct KeyEvent {
        dk::input::eInputKey   InputKey;
        int32_t                 IsDown; // Makes the struct 8 bytes aligned
    };

    struct AxisEvent {
        dk::input::eInputAxis  InputAxis;
        uint32_t                __PADDING__;
        double                  RawAxisValue;
    };

public:
                                InputReader();
                                InputReader( InputReader& ) = delete;
                                ~InputReader();

    void                        create();
    void                        onFrame( InputMapper* inputMapper );
    dk::input::eInputLayout    getActiveInputLayout() const;

    void                        pushKeyEvent( InputReader::KeyEvent&& keyEvent );
    void                        pushAxisEvent( InputReader::AxisEvent&& axisEvent );

    void                        pushKeyStroke( const dk::input::eInputKey keyStroke );
    fnKeyStrokes_t              getAndFlushKeyStrokes();

    void                        setAbsoluteAxisValue( const dk::input::eInputAxis axis, const double absoluteValue );
    double                      getAbsoluteAxisValue( const dk::input::eInputAxis axis );

private:
    dk::input::eInputLayout        activeInputLayout;
    std::queue<KeyEvent>            inputKeyEventQueue;
    std::queue<AxisEvent>           inputAxisEventQueue;

    fnKeyStrokes_t                  textInput;

    double                          absoluteAxisValues[dk::input::eInputAxis::InputAxis_COUNT];
    bool                            buttonMap[dk::input::KEYBOARD_KEY_COUNT];
};
