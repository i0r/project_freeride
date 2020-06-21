/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FileSystemObject;

#include <Parsing/ImageParser.h>
#include <vector>

struct DirectDrawSurface
{
    // Description of the parsed DDS.
    ParsedImageDesc         TextureDescription;

    // Texels for this DDS. Careful: the data layout is defined by TextureDescription::Format (check the field first
    // to figure out how to read the pixels; you don't need to do anything if you upload the texture to the GPU).
    std::vector<uint8_t>    TextureData;
};

namespace dk
{
    namespace io
    {
        void LoadDirectDrawSurface( FileSystemObject* stream, DirectDrawSurface& data );
        //void SaveDirectDrawSurface( FileSystemObject* stream, std::vector<f32>& texels, const TextureDescription& description );
    }
}
