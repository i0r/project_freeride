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

void CommandList::transitionImage( Image& image, const eResourceState state, const u32 mipIndex, const TransitionType transitionType )
{

}

void CommandList::insertComputeBarrier( Image& image )
{

}
#endif
