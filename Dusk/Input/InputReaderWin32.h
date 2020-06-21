/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#ifdef DUSK_WIN

#include "InputLayouts.h"

class InputReader;
namespace dk
{
    namespace input
    {   
        void CreateInputReaderImpl( dk::input::eInputLayout& activeInputLayout );
        void ProcessInputEventImpl( InputReader* inputReader, const HWND win, const WPARAM infos, const uint64_t timestamp );
    }
}

#endif
