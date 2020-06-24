/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include <Rendering/CommandList.h>
#include "CommandList.h"

#include <d3d11.h>
#include <d3d11_1.h>

// Memory allocated for commands buffering (in bytes).
static constexpr i32 COMMAND_PACKET_SIZE = 2048 << 4;

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
    dk::core::free( memoryAllocator, nativeCommandList );
}

void CommandList::initialize( BaseAllocator* allocator, const i32 pooledIndex )
{
    memoryAllocator = allocator;
    commandListPoolIndex = pooledIndex;

    LinearAllocator* cmdPacketAllocator = dk::core::allocate<LinearAllocator>( memoryAllocator, COMMAND_PACKET_SIZE, memoryAllocator->allocate( COMMAND_PACKET_SIZE ) );
    nativeCommandList = dk::core::allocate<NativeCommandList>( memoryAllocator, cmdPacketAllocator );
}

void CommandList::end()
{
    CommandPacket::ArgumentLessPacket* commandPacket = dk::core::allocate<CommandPacket::ArgumentLessPacket>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_END;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::setViewport( const Viewport& viewport )
{
    CommandPacket::SetViewport* commandPacket = dk::core::allocate<CommandPacket::SetViewport>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_SET_VIEWPORT;
    commandPacket->ViewportObject = viewport;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::setScissor( const ScissorRegion& scissorRegion )
{
    CommandPacket::SetScissor* commandPacket = dk::core::allocate<CommandPacket::SetScissor>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_SET_SCISSOR;
    commandPacket->ScissorObject = scissorRegion;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::draw( const u32 vertexCount, const u32 instanceCount, const u32 vertexOffset, const u32 instanceOffset )
{
    CommandPacket::Draw* commandPacket = dk::core::allocate<CommandPacket::Draw>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_DRAW;
    commandPacket->VertexCount = vertexCount;
    commandPacket->InstanceCount = instanceCount;
    commandPacket->VertexOffset = vertexOffset;
    commandPacket->InstanceOffset = instanceOffset;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::drawIndexed( const u32 indiceCount, const u32 instanceCount, const u32 indiceOffset, const u32 vertexOffset, const u32 instanceOffset )
{
    CommandPacket::DrawIndexed* commandPacket = dk::core::allocate<CommandPacket::DrawIndexed>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_DRAW_INDEXED;
    commandPacket->IndexCount = indiceCount;
    commandPacket->InstanceCount = instanceCount;
    commandPacket->IndexOffset = indiceOffset;
    commandPacket->VertexOffset = vertexOffset;
    commandPacket->InstanceOffset = instanceOffset;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::dispatchCompute( const u32 threadCountX, const u32 threadCountY, const u32 threadCountZ )
{
    CommandPacket::Dispatch* commandPacket = dk::core::allocate<CommandPacket::Dispatch>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_DISPATCH;
    commandPacket->ThreadCountX = threadCountX;
    commandPacket->ThreadCountY = threadCountY;
    commandPacket->ThreadCountZ = threadCountZ;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::pushEventMarker( const dkChar_t* eventName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    CommandPacket::PushEvent* commandPacket = dk::core::allocate<CommandPacket::PushEvent>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_PUSH_EVENT;
    commandPacket->EventName = eventName;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
#endif
}

void CommandList::popEventMarker()
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    CommandPacket::ArgumentLessPacket* commandPacket = dk::core::allocate<CommandPacket::ArgumentLessPacket>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_POP_EVENT;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
#endif
}
#endif
