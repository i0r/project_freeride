/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "CommandList.h"

#include "RenderDevice.h"

void CommandList::setResourceFrameIndex( const i32 frameIndex )
{
    resourceFrameIndex = ( frameIndex % RenderDevice::PENDING_FRAME_COUNT );
}
