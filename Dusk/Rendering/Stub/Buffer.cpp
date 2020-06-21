/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
Buffer* RenderDevice::createBuffer( const BufferDesc& description, const void* initialData )
{
    return nullptr;
}

void RenderDevice::destroyBuffer( Buffer* buffer )
{

}

void RenderDevice::setDebugMarker( Buffer& buffer, const dkChar_t* objectName )
{

}

void CommandList::bindVertexBuffer( const Buffer** buffers, const u32 bufferCount, const u32 startBindIndex )
{

}

void CommandList::bindIndiceBuffer( const Buffer* buffer, const bool use32bitsIndices )
{

}

void CommandList::updateBuffer( Buffer& buffer, const void* data, const size_t dataSize )
{

}

void* CommandList::mapBuffer( Buffer& buffer, const u32 startOffsetInBytes, const u32 sizeInBytes )
{
    return nullptr;
}

void CommandList::unmapBuffer( Buffer& buffer )
{

}

void CommandList::transitionBuffer( Buffer& buffer, const eResourceState state )
{

}
#endif
