/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "RenderDevice.h"

RenderDevice::RenderDevice( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , pipelineStateCacheAllocator( nullptr )
    , renderContext( nullptr )
    , swapChainImage( nullptr )
    , frameIndex( 0ull )
{
    memset( graphicsCmdListAllocator, 0, sizeof( LinearAllocator* ) * PENDING_FRAME_COUNT );
    memset( computeCmdListAllocator, 0, sizeof( LinearAllocator* ) * PENDING_FRAME_COUNT );
}

size_t RenderDevice::getFrameIndex() const
{
    return frameIndex;
}

Image* RenderDevice::getSwapchainBuffer()
{
    return swapChainImage;
}
