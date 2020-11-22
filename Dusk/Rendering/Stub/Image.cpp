/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
Image* RenderDevice::createImage( const ImageDesc& description, const void* initialData, const size_t initialDataSize )
{
    return nullptr;
}

void RenderDevice::destroyImage( Image* image )
{

}

void RenderDevice::setDebugMarker( Image& image, const dkChar_t* objectName )
{

}

void RenderDevice::createImageView( Image& image, const ImageViewDesc& viewDescription, const u32 creationFlags )
{

}

void CommandList::transitionImage( Image& image, const eResourceState state, const u32 mipIndex, const TransitionType transitionType )
{

}

void CommandList::insertComputeBarrier( Image& image )
{

}

void CommandList::setupFramebuffer( FramebufferAttachment* renderTargetViews, FramebufferAttachment depthStencilView )
{

}

void CommandList::clearRenderTargets( Image** renderTargetViews, const u32 renderTargetCount, const f32 clearValues[4] )
{

}

void CommandList::clearDepthStencil( Image* depthStencilView, const f32 clearValue, const bool clearStencil, const u8 clearStencilValue )
{

}

void CommandList::resolveImage( Image& src, Image& dst )
{

}
#endif
