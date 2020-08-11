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

void DisplaySurface::changeDisplayMode( const eDisplayMode newDisplayMode )
{
    switch ( newDisplayMode ) {
    case WINDOWED:
        if ( displayMode == FULLSCREEN ) {
            setFullscreenDisplayMode();
		} else if ( displayMode == FULLSCREEN ) {
			setFullscreenBorderlessDisplayMode();
		}
        break;
	case FULLSCREEN:
		setFullscreenDisplayMode();
        break;
	case BORDERLESS:
		setFullscreenBorderlessDisplayMode();
        break;
    }

    displayMode = newDisplayMode;
}
