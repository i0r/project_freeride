/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
#include "Rendering/RenderDevice.h"
#include "Rendering/CommandList.h"

RenderDevice::~RenderDevice()
{

}

void RenderDevice::create( DisplaySurface& displaySurface, const u32 desiredRefreshRate, const bool useDebugContext)
{
    DUSK_UNUSED_VARIABLE( displaySurface );
    DUSK_UNUSED_VARIABLE( desiredRefreshRate );
    DUSK_UNUSED_VARIABLE( useDebugContext );
}

void RenderDevice::enableVerticalSynchronisation( const bool enabled )
{
    DUSK_UNUSED_VARIABLE( enabled );
}

CommandList& RenderDevice::allocateGraphicsCommandList()
{
    static CommandList STUB_CMDLIST_GFX( CommandList::Type::GRAPHICS );
    return STUB_CMDLIST_GFX;
}

CommandList& RenderDevice::allocateComputeCommandList()
{
    static CommandList STUB_CMDLIST_COMPUTE( CommandList::Type::COMPUTE );
    return STUB_CMDLIST_COMPUTE;
}

CommandList& RenderDevice::allocateCopyCommandList()
{
    static CommandList STUB_CMDLIST_COPY( CommandList::Type::COMPUTE );
    return STUB_CMDLIST_COPY;
}

void RenderDevice::submitCommandList( CommandList& cmdList )
{
    DUSK_UNUSED_VARIABLE( cmdList );
}

void RenderDevice::submitCommandLists( CommandList** cmdLists, const u32 cmdListCount )
{

}

void RenderDevice::present()
{

}

void RenderDevice::waitForPendingFrameCompletion()
{

}

const dkChar_t* RenderDevice::getBackendName()
{
    return DUSK_STRING( "Headless" );
}

void RenderDevice::resizeBackbuffer( const u32 width, const u32 height )
{

}
#endif
