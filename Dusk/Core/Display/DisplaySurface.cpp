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
    , width( 0u )
    , height( 0u )
    , originX( 0 )
    , originY( 0 )
    , monitorIndex( 0u )
{

}

void DisplaySurface::changeDisplayMode( const eDisplayMode newDisplayMode )
{
    switch ( newDisplayMode ) {
    case WINDOWED:
        setWindowedDisplayMode();
        break;
	case FULLSCREEN:
		setFullscreenDisplayMode();
        break;
	case BORDERLESS:
		setFullscreenBorderlessDisplayMode();
        break;
    default:
        DUSK_DEV_ASSERT( false, "Invalid Usage!" );
        break;
    }

    displayMode = newDisplayMode;
}
