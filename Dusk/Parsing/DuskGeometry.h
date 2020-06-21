/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/

namespace dk
{
    // The dword magic (first dword in the header).
    constexpr u32 DuskGeometryMagic = MakeFourCC( 'G', 'E', 'O', 'M' );

    enum class DuskGeometryVersion : u16 {
        Version_1_0_0_0 = 0,

        // Do not add anything below this line!
        Version_Count,
        LatestVersion = ( Version_Count - 1 )
    };
}