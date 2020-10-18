/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include "CommandList.h"
#include <Rendering/CommandList.h>

#include <d3d12.h>
#include <pix.h>

NativeCommandList::NativeCommandList()
    : graphicsCmdList( nullptr )
    , currentCommandSignature( nullptr )
    , allocator( nullptr )
{

}

NativeCommandList::~NativeCommandList()
{
    graphicsCmdList->Release();
}

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
    nativeCommandList = dk::core::allocate<NativeCommandList>( memoryAllocator );
    commandListPoolIndex = pooledIndex;
}

void CommandList::end()
{
    nativeCommandList->graphicsCmdList->Close();
}

void CommandList::setViewport( const Viewport& viewport )
{
    D3D12_VIEWPORT nativeViewport;
    nativeViewport.TopLeftX = static_cast<FLOAT>( viewport.X );
    nativeViewport.TopLeftY = static_cast<FLOAT>( viewport.Y );
    nativeViewport.Width = static_cast< FLOAT >( viewport.Width );
    nativeViewport.Height = static_cast< FLOAT >( viewport.Height );
    nativeViewport.MinDepth = static_cast< FLOAT >( viewport.MinDepth );
    nativeViewport.MaxDepth = static_cast< FLOAT >( viewport.MaxDepth );

    nativeCommandList->graphicsCmdList->RSSetViewports( 1, &nativeViewport );
}

void CommandList::setScissor( const ScissorRegion& scissorRegion )
{
    D3D12_RECT nativeScissor;
    nativeScissor.bottom = scissorRegion.Bottom;
    nativeScissor.top = scissorRegion.Top;
    nativeScissor.left = scissorRegion.Left;
    nativeScissor.right = scissorRegion.Right;

    nativeCommandList->graphicsCmdList->RSSetScissorRects( 1, &nativeScissor );
}

void CommandList::draw( const u32 vertexCount, const u32 instanceCount, const u32 vertexOffset, const u32 instanceOffset )
{
    nativeCommandList->graphicsCmdList->DrawInstanced( vertexCount, instanceCount, vertexOffset, instanceOffset );
}

void CommandList::drawIndexed( const u32 indiceCount, const u32 instanceCount, const u32 indiceOffset, const u32 vertexOffset, const u32 instanceOffset )
{
    nativeCommandList->graphicsCmdList->DrawIndexedInstanced( indiceCount, instanceCount, indiceOffset, vertexOffset, instanceOffset );
}

void CommandList::dispatchCompute( const u32 threadCountX, const u32 threadCountY, const u32 threadCountZ )
{
    nativeCommandList->graphicsCmdList->Dispatch( threadCountX, threadCountY, threadCountZ );
}

void CommandList::multiDrawIndexedInstancedIndirect( const u32 instanceCount, Buffer* argsBuffer, const u32 bufferAlignmentInBytes /* = 0u */, const u32 argumentsSizeInBytes /* = 0u */ )
{
 
}

void CommandList::pushEventMarker( const dkChar_t* eventName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    PIXBeginEvent( nativeCommandList->graphicsCmdList, PIX_COLOR( 255, 0, 0 ), eventName );
#endif
}

void CommandList::popEventMarker()
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    PIXEndEvent( nativeCommandList->graphicsCmdList );
#endif
}
#endif
