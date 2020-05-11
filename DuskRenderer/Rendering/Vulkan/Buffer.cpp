/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Core/StringHelpers.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "Buffer.h"
#include "ResourceAllocationHelpers.h"

#include <vulkan/vulkan.h>

Buffer* RenderDevice::createBuffer( const BufferDesc& description, const void* initialData )
{
    VkBufferCreateInfo bufferInfo;
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = description.SizeInBytes;
    bufferInfo.usage = GetResourceUsageFlags( description.BindFlags );
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkMemoryPropertyFlags allocFlags = GetMemoryPropertyFlags( description.Usage );

    Buffer* buffer = dk::core::allocate<Buffer>( memoryAllocator );
    buffer->Stride = description.StrideInBytes;

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        VkBuffer nativeBuffer;

        VkResult bufferCreationResult = vkCreateBuffer( renderContext->device, &bufferInfo, nullptr, &nativeBuffer );
        DUSK_ASSERT( ( bufferCreationResult == VK_SUCCESS ), "Failed to create buffer description! (error code: 0x%x)\n", bufferCreationResult )

        // Allocate memory from the device
        VkDeviceMemory deviceMemory;

        VkMemoryRequirements bufferMemoryRequirements;
        vkGetBufferMemoryRequirements( renderContext->device, nativeBuffer, &bufferMemoryRequirements );

        VkMemoryAllocateInfo allocInfo;
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.allocationSize = bufferMemoryRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(
                    renderContext->physicalDevice,
                    bufferMemoryRequirements.memoryTypeBits,
                    allocFlags
        );

        VkResult allocationResult = vkAllocateMemory( renderContext->device, &allocInfo, nullptr, &deviceMemory );
        DUSK_ASSERT( ( allocationResult == VK_SUCCESS ), "Failed to allocate VkBuffer memory (error code: 0x%x)\n", allocationResult )

        vkBindBufferMemory( renderContext->device, nativeBuffer, deviceMemory, 0ull );

        buffer->resource[i] = nativeBuffer;
        buffer->deviceMemory[i] = deviceMemory;
    }

    /*VkBufferView bufferView = nullptr;
    if ( bufferInfo.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
      || bufferInfo.usage & VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ) {
        VkBufferViewCreateInfo bufferViewDesc;
        bufferViewDesc.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        bufferViewDesc.pNext = nullptr;
        bufferViewDesc.flags = 0u;
        bufferViewDesc.buffer = nativeBuffer;
        bufferViewDesc.format = VK_IMAGE_FORMAT[description.viewFormat];
        bufferViewDesc.offset = 0ull;
        bufferViewDesc.range = VK_WHOLE_SIZE;

        vkCreateBufferView( renderContext->device, &bufferViewDesc, nullptr, &bufferView );
    }
    buffer->bufferView = bufferView;

    if ( initialData != nullptr ) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        _createBuffer( renderContext, bufferInfo.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory );

        void* data;
        vkMapMemory( renderContext->device, stagingBufferMemory, 0, bufferInfo.size, 0, &data );
        memcpy( data, initialData, bufferInfo.size );
        vkUnmapMemory( renderContext->device, stagingBufferMemory );

        copyBuffer( renderContext, stagingBuffer, nativeBuffer, bufferInfo.size );

        vkDestroyBuffer( renderContext->device, stagingBuffer, nullptr );
        vkFreeMemory( renderContext->device, stagingBufferMemory, nullptr );
    }*/

    return buffer;
}

void RenderDevice::destroyBuffer( Buffer* buffer )
{
    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        vkDestroyBuffer( renderContext->device, buffer->resource[i], nullptr );
    }

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        vkFreeMemory( renderContext->device, buffer->deviceMemory[i], nullptr );
    }

    if ( buffer->bufferView != nullptr ) {
        vkDestroyBufferView( renderContext->device, buffer->bufferView, nullptr );
    }

    dk::core::free( memoryAllocator, buffer );
}

void RenderDevice::setDebugMarker( Buffer& buffer, const dkChar_t* objectName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    VkDebugMarkerObjectNameInfoEXT dbgMarkerObjName = {};
    dbgMarkerObjName.sType = VkStructureType::VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    dbgMarkerObjName.pNext = nullptr;
    dbgMarkerObjName.objectType = VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
    dbgMarkerObjName.pObjectName = WideStringToString( objectName ).c_str();

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        dbgMarkerObjName.object = reinterpret_cast< uint64_t >( buffer.resource[i] );
        renderContext->debugObjectMarker( renderContext->device, &dbgMarkerObjName );
    }
#endif
}

void CommandList::bindVertexBuffer( const Buffer** buffers, const u32 bufferCount, const u32 startBindIndex )
{
    VkDeviceSize offsets[8];
    VkBuffer nativeBuffers[8];

    for ( u32 i = 0; i < bufferCount; i++ ) {
        offsets[i] = 0;
        nativeBuffers[i] = buffers[i]->resource[resourceFrameIndex];
    }

    vkCmdBindVertexBuffers( nativeCommandList->cmdList, startBindIndex, bufferCount, nativeBuffers, offsets );
}

void CommandList::bindIndiceBuffer( const Buffer* buffer, const bool use32bitsIndices )
{
    VkIndexType indexType = ( use32bitsIndices ) ? VkIndexType::VK_INDEX_TYPE_UINT32 : VkIndexType::VK_INDEX_TYPE_UINT16;

    vkCmdBindIndexBuffer( nativeCommandList->cmdList, buffer->resource[resourceFrameIndex], 0, indexType );
}

void CommandList::updateBuffer( Buffer& buffer, const void* data, const size_t dataSize )
{
    DUSK_RAISE_FATAL_ERROR( !nativeCommandList->isInRenderPass, "Trying to call vkCmdUpdateBuffer inside a RenderPass!" );

    vkCmdUpdateBuffer( nativeCommandList->cmdList, buffer.resource[resourceFrameIndex], 0ull, dataSize, data );
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
