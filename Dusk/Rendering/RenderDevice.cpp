/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "RenderDevice.h"

DUSK_ENV_VAR( DisableVendorExtensions, false, bool ); // If true, disable the usage of graphics vendor extension (AMD/Nvidia/Intel/etc.).
DUSK_ENV_VAR( AutomaticOutputSelection, false, bool ); // If true, automatically chose the output for swapchain creation (overrides user's monitor request).
DUSK_DEV_VAR_PERSISTENT( UseSoftwareRasterization, false, bool ) // Force WARP adapter/Software rasterizer usage (e.g. in case no D3D12 compatible adapter is available)

RenderDevice::RenderDevice( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , pipelineStateCacheAllocator( nullptr )
    , renderContext( nullptr )
    , swapChainImage( nullptr )
    , frameIndex( 0ull )
    , refreshRate( -1 )
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

u32 RenderDevice::getActiveRefreshRate() const
{
    return refreshRate;
}
