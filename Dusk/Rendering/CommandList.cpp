/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "CommandList.h"

#include "RenderDevice.h"

void CommandList::setFrameIndex( const i32 deviceFrameIndex )
{
    resourceFrameIndex = ( deviceFrameIndex % RenderDevice::PENDING_FRAME_COUNT );
    frameIndex = deviceFrameIndex;
}
