/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

namespace dk
{
    namespace input
    {
#define InputLayout( option )\
        option( INPUT_LAYOUT_QWERTY )\
        option( INPUT_LAYOUT_AZERTY )\
        option( INPUT_LAYOUT_QWERTZ )

        DUSK_LAZY_ENUM( InputLayout )
#undef InputLayout
    }
}
