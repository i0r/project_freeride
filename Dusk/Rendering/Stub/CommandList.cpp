/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
#include "Rendering/CommandList.h"

CommandList::CommandList( const CommandList::Type cmdListType )
    : memoryAllocator( nullptr )
    , nativeCommandList( nullptr )
    , commandListType( cmdListType )
    , resourceFrameIndex( 0 )
    , commandListPoolIndex( 0 )
{

}

CommandList::~CommandList()
{

}

void CommandList::initialize( BaseAllocator* allocator, const i32 pooledIndex )
{

}

void CommandList::end()
{

}

void CommandList::setViewport( const Viewport& viewport )
{

}

void CommandList::setScissor( const ScissorRegion& scissorRegion )
{

}

void CommandList::draw( const u32 vertexCount, const u32 instanceCount, const u32 vertexOffset, const u32 instanceOffset )
{

}

void CommandList::drawIndexed( const u32 indiceCount, const u32 instanceCount, const u32 indiceOffset, const u32 vertexOffset, const u32 instanceOffset )
{

}

void CommandList::dispatchCompute( const u32 threadCountX, const u32 threadCountY, const u32 threadCountZ )
{

}

void CommandList::pushEventMarker( const dkChar_t* eventName )
{

}

void CommandList::popEventMarker()
{

}

void CommandList::multiDrawIndexedInstancedIndirect( const u32 instanceCount, Buffer* argsBuffer, const u32 bufferAlignmentInBytes /* = 0u */, const u32 argumentsSizeInBytes /* = 0u */ )
{

}
#endif
