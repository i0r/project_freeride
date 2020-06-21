/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
RenderDevice::~RenderDevice()
{

}

void RenderDevice::create( DisplaySurface& displaySurface, const bool useDebugContext )
{
    DUSK_UNUSED_VARIABLE( displaySurface );
    DUSK_UNUSED_VARIABLE( useDebugContext );
}

void RenderDevice::enableVerticalSynchronisation( const bool enabled )
{
    DUSK_UNUSED_VARIABLE( enabled );
}

CommandList& RenderDevice::allocateGraphicsCommandList()
{
    static CommandList STUB_CMDLIST_GFX( CommandList::GRAPHICS );
    return STUB_CMDLIST_GFX;
}

CommandList& RenderDevice::allocateComputeCommandList()
{
    static CommandList STUB_CMDLIST_COMPUTE( CommandList::COMPUTE );
    return STUB_CMDLIST_COMPUTE;
}

void RenderDevice::submitCommandList( CommandList& cmdList )
{
    DUSK_UNUSED_VARIABLE( cmdList );
}

void RenderDevice::present()
{

}

void RenderDevice::waitForPendingFrameCompletion()
{

}

const dkChar_t* RenderDevice::getBackendName()
{
    return DUSK_STRING( "Stub" );
}
#endif
