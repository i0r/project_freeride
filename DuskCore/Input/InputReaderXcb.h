/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#ifdef DUSK_UNIX

#include "InputLayouts.h"

#include <xcb/xcb.h>

class InputReader;
namespace dk
{
    namespace input
    {   
        void CreateInputReaderImpl( dk::input::eInputLayout& activeInputLayout );
        void ProcessInputEventImpl( InputReader* inputReader, const xcb_generic_event_t* event );
    }
}

#endif
