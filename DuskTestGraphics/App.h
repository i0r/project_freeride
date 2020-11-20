/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#define WIN_MODE_OPTION_LIST( option ) option( WINDOWED_MODE ) option( FULLSCREEN_MODE ) option( BORDERLESS_MODE )
DUSK_ENV_OPTION_LIST( WindowMode, WIN_MODE_OPTION_LIST )

namespace dk
{
    namespace test
    {
        void Start( const char* cmdLineArgs );
    }
}
