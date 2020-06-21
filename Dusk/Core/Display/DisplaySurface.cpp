/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "DisplaySurface.h"

DisplaySurface::DisplaySurface( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , displaySurface( nullptr )
    , displayMode( eDisplayMode::WINDOWED )
{

}
