/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FileSystemObject;

#include <Rendering/Shared.h>
#include <vector>

struct DirectDrawSurface
{
    ImageDesc                   imageDesc;
    std::vector<uint8_t>    textureData;
};

namespace dk
{
    namespace io
    {
        void LoadDirectDrawSurface( FileSystemObject* stream, DirectDrawSurface& data );
        //void SaveDirectDrawSurface( FileSystemObject* stream, std::vector<f32>& texels, const TextureDescription& description );
    }
}
