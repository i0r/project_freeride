/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN
#include <Rendering/CommandList.h>
#include "CommandList.h"

#include <Core/StringHelpers.h>

#include <string.h> // used by memset

NativeCommandList::NativeCommandList()
    : cmdList( VK_NULL_HANDLE )
    , vkCmdDebugMarkerBegin( nullptr )
    , vkCmdDebugMarkerEnd( nullptr )
{

}

NativeCommandList::~NativeCommandList()
{
    cmdList = VK_NULL_HANDLE;
    vkCmdDebugMarkerBegin = nullptr;
    vkCmdDebugMarkerEnd = nullptr;
}

CommandList::CommandList( const CommandList::Type cmdListType )
    : nativeCommandList( nullptr )
    , memoryAllocator( nullptr )
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
    if ( nativeCommandList->isInRenderPass ) {
        vkCmdEndRenderPass( nativeCommandList->cmdList );
    }
}

void CommandList::setViewport( const Viewport& viewport )
{
    DUSK_DEV_ASSERT( commandListType == CommandList::Type::GRAPHICS, "Called setViewport on a compute command list! The call will be ignored." );

    nativeCommandList->activeViewport = viewport;

    VkViewport vkViewport;
    vkViewport.x = static_cast< f32 >( viewport.X );
    vkViewport.y = static_cast< f32 >( viewport.Y );
    vkViewport.width = static_cast< f32 >( viewport.Width );
    vkViewport.height = static_cast< f32 >( viewport.Height );
    vkViewport.minDepth = viewport.MinDepth;
    vkViewport.maxDepth = viewport.MaxDepth;

    vkCmdSetViewport( nativeCommandList->cmdList, 0, 1, &vkViewport );
}

void CommandList::setScissor( const ScissorRegion& scissorRegion )
{
    DUSK_DEV_ASSERT( commandListType == CommandList::Type::GRAPHICS, "Called setScissor on a compute command list! The call will be ignored." );

    VkRect2D vkScissor;
    vkScissor.offset.x = scissorRegion.Left;
    vkScissor.offset.y = scissorRegion.Top;
    vkScissor.extent.width = static_cast<uint32_t>( scissorRegion.Right );
    vkScissor.extent.height = static_cast<uint32_t>( scissorRegion.Bottom );

    vkCmdSetScissor( nativeCommandList->cmdList, 0, 1, &vkScissor );
}

void CommandList::draw( const u32 vertexCount, const u32 instanceCount, const u32 vertexOffset, const u32 instanceOffset )
{
    vkCmdDraw( nativeCommandList->cmdList, vertexCount, instanceCount, vertexOffset, instanceOffset );
}

void CommandList::drawIndexed( const u32 indiceCount, const u32 instanceCount, const u32 indiceOffset, const u32 vertexOffset, const u32 instanceOffset )
{
    vkCmdDrawIndexed( nativeCommandList->cmdList, indiceCount, instanceCount, indiceOffset, static_cast<int32_t>( vertexOffset ), instanceOffset );
}

void CommandList::dispatchCompute( const u32 threadCountX, const u32 threadCountY, const u32 threadCountZ )
{
    vkCmdDispatch( nativeCommandList->cmdList, threadCountX, threadCountY, threadCountZ );
}

void CommandList::multiDrawIndexedInstancedIndirect( const u32 instanceCount, Buffer* argsBuffer, const u32 bufferAlignmentInBytes, const u32 argumentsSizeInBytes )
{

}

void CommandList::pushEventMarker( const dkChar_t* eventName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    markerInfo.pNext = nullptr;
    memset( markerInfo.color, 0, sizeof( float ) * 4 );
    markerInfo.pMarkerName = DUSK_NARROW_STRING( eventName ).c_str();

    nativeCommandList->vkCmdDebugMarkerBegin( nativeCommandList->cmdList, &markerInfo );
#endif
}

void CommandList::popEventMarker()
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    nativeCommandList->vkCmdDebugMarkerEnd( nativeCommandList->cmdList );
#endif
}
#endif
