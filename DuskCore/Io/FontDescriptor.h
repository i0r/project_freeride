/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <vector>

class FileSystemObject;

struct FontDescriptor {
    dkString_t  Name;
    uint32_t    AtlasWidth;
    uint32_t    AtlasHeight;

    struct Glyph {
        int32_t PositionX;
        int32_t PositionY;
        int32_t Width;
        int32_t Height;
        int32_t OffsetX;
        int32_t OffsetY;
        int32_t AdvanceX;
    };

    std::vector<Glyph> Glyphes;
};

namespace dk
{
    namespace io
    {
        void LoadFontFile( FileSystemObject* file, FontDescriptor& data );
    }
}
