/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/

#include <Core/ViewFormat.h>

struct ParsedImageDesc 
{
    enum class Dimension {
        DIMENSION_UNKNOWN,

        DIMENSION_1D,
        DIMENSION_2D,
        DIMENSION_3D
    };

    // Parsed image dimension.
    Dimension       ImageDimension;

    // Format of the image (data layout).
    eViewFormat     Format;

    // Width of the image.
    u32             Width;

    // Height of the image (ignored for 1D images).
    u32             Height;

    // Depth of the image (ignored for 1D and 2D images).
    u32             Depth;

    // Length of the array (if the image is an array of images).
    u32             ArraySize;

    // Number of mip for this image (if this image has a precomputed mipchain).
    u32             MipCount;

    // True if this image is a cubemap; false otherwise.
    u8              IsCubemap : 1;

    ParsedImageDesc( const Dimension dimension = Dimension::DIMENSION_UNKNOWN )
        : ImageDimension( dimension )
        , Format( eViewFormat::VIEW_FORMAT_UNKNOWN )
        , Width( 0 )
        , Height( 0 )
        , Depth( 0 )
        , ArraySize( 1 )
        , MipCount( 1 )
        , IsCubemap( false )
    {

    }
};
