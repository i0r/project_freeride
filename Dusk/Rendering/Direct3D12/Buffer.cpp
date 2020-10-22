/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "Buffer.h"
#include "ResourceAllocationHelpers.h"

#include "RenderDevice.h"
#include "CommandList.h"

#include <d3d12.h>

Buffer* RenderDevice::createBuffer( const BufferDesc& description, const void* initialData )
{
    size_t alignedSize = ( description.SizeInBytes < 256 ) 
        ? 256 
        : ( description.SizeInBytes + ( 256 - ( description.SizeInBytes % 256 ) ) );

    D3D12_RESOURCE_DESC resourceDesc;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = GetNativeResourceFlags( description.BindFlags );

    ID3D12Device* device = renderContext->device;
    D3D12_RESOURCE_STATES stateFlags = ( initialData == nullptr ) ? GetResourceStateFlags( description.BindFlags ) : D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
    
    Buffer* buffer = dk::core::allocate<Buffer>( memoryAllocator );
    buffer->size = static_cast<u32>( alignedSize );
    buffer->Stride = description.StrideInBytes;
    buffer->usage = description.Usage;
    buffer->heapOffset = 0;
    buffer->allocationRequest = nullptr;

    memset( buffer->memoryMappedRanges, 0, sizeof( D3D12_RANGE ) * Buffer::MAX_SIMULTANEOUS_MEMORY_MAPPING );
    buffer->memoryMappedRangeCount = 0;

    D3D12_RESOURCE_ALLOCATION_INFO allocInfos;
    HRESULT operationResult = S_OK;
    switch ( description.Usage ) {
    case RESOURCE_USAGE_STATIC: {
        operationResult = device->CreatePlacedResource(
            renderContext->staticBufferHeap,
            renderContext->heapOffset,
            &resourceDesc,
            stateFlags,
            nullptr,
            __uuidof( ID3D12Resource ),
            reinterpret_cast< void** >( &buffer->resource[0] )
        );
        DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Buffer creation FAILED! (error code: 0x%x)", operationResult );

        for ( i32 i = 1; i < PENDING_FRAME_COUNT; i++ ) {
            buffer->resource[i] = buffer->resource[0];
        }

        // TODO Should we take alignment in account? Or assume every allocation will be memory aligned by default?
        allocInfos = device->GetResourceAllocationInfo( 0, 1, &resourceDesc );
        renderContext->heapOffset += allocInfos.SizeInBytes;
    } break;
    case RESOURCE_USAGE_STAGING: {
        D3D12_HEAP_PROPERTIES readbackHeapProperties;
        readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
        readbackHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        readbackHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        readbackHeapProperties.CreationNodeMask = 0;
        readbackHeapProperties.VisibleNodeMask = 0;

        operationResult = device->CreateCommittedResource(
            &readbackHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            __uuidof( ID3D12Resource ),
            reinterpret_cast< void** >( &buffer->resource[0] )
        );
        DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Buffer creation FAILED! (error code: 0x%x)", operationResult );

        for ( i32 i = 1; i < PENDING_FRAME_COUNT; i++ ) {
            buffer->resource[i] = buffer->resource[0];
        }
    } break;
    case RESOURCE_USAGE_DEFAULT: {
        //alignedSize += ( D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT - ( alignedSize % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT ) );

        for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
            operationResult = device->CreatePlacedResource(
                renderContext->dynamicBufferHeap,
                renderContext->dynamicBufferHeapPerFrameCapacity * i + renderContext->dynamicBufferHeapOffset,
                &resourceDesc,
                stateFlags,
                nullptr,
                __uuidof( ID3D12Resource ),
                reinterpret_cast< void** >( &buffer->resource[i] )
            );

            // TODO Should we take alignment in account? Or assume every allocation will be memory aligned by default?
            allocInfos = device->GetResourceAllocationInfo( 0, 1, &resourceDesc );
            renderContext->dynamicBufferHeapOffset += allocInfos.SizeInBytes;
            renderContext->dynamicBufferHeapOffset += ( D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT - ( renderContext->dynamicBufferHeapOffset % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT ) );

            DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Buffer creation FAILED! (error code: 0x%x)", operationResult );
        }
    } break;
    case RESOURCE_USAGE_DYNAMIC: {
        // Keep a reference to the context volatile buffers in use
        for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
            buffer->resource[i] = renderContext->volatileBuffers[i];
        }

        Buffer** bufferAllocationRequest = dk::core::allocate<Buffer*>( renderContext->volatileAllocatorsPool );
        *bufferAllocationRequest = buffer;
    } break;
    }

    if ( initialData != nullptr && description.Usage != RESOURCE_USAGE_DYNAMIC ) {
        D3D12_HEAP_PROPERTIES uploadHeapProperties;
        uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProperties.CreationNodeMask = 0;
        uploadHeapProperties.VisibleNodeMask = 0;

        // Create a buffer for upload only
        // TODO How to optimize this correctly?
        //  > Do all buffer/image upload asynchronously, exclusively using the copy cmd queue?
        //  > Do the upload on a shared heap (using stack/watermark allocator?)
        //  > Other? (do research)
        D3D12_RESOURCE_DESC resourceUploadDesc;
        resourceUploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceUploadDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        resourceUploadDesc.Width = allocInfos.SizeInBytes;
        resourceUploadDesc.Height = 1;
        resourceUploadDesc.DepthOrArraySize = 1;
        resourceUploadDesc.MipLevels = 1;
        resourceUploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceUploadDesc.SampleDesc.Count = 1;
        resourceUploadDesc.SampleDesc.Quality = 0;
        resourceUploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceUploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ID3D12Resource* uploadResource = nullptr;
        HRESULT operationResult = device->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceUploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof( ID3D12Resource ),
            reinterpret_cast< void** >( &uploadResource )
        );

        // Upload data from CPU to GPU (upload heap)
        void* pData = nullptr;

        D3D12_RANGE readRange;
        readRange.Begin = 0;
        readRange.End = 0;

        uploadResource->Map( 0, &readRange, &pData );
        memcpy( pData, initialData, alignedSize );

        // Allocate a cmd list on the copy command queue in order to perform texel copy
        size_t resourceIdx = frameIndex % PENDING_FRAME_COUNT;
        size_t cmdListIdx = renderContext->copyCmdListUsageIndex[resourceIdx];
        ID3D12CommandAllocator* cmdListAllocator = renderContext->copyCmdAllocator[resourceIdx][cmdListIdx];
        ID3D12GraphicsCommandList* copyCmdList = renderContext->copyCmdLists[resourceIdx][cmdListIdx];
        cmdListAllocator->Reset();
        copyCmdList->Reset( cmdListAllocator, nullptr );

        i32 resourceCount = ( buffer->usage == RESOURCE_USAGE_DEFAULT ) ? RenderDevice::PENDING_FRAME_COUNT : 1;
        for ( i32 i = 0; i < resourceCount; i++ ) {
            D3D12_RESOURCE_BARRIER transitionDstBarrier;
            transitionDstBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            transitionDstBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            transitionDstBarrier.Transition.pResource = buffer->resource[i];
            transitionDstBarrier.Transition.StateBefore = stateFlags;
            transitionDstBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            transitionDstBarrier.Transition.Subresource = 0;
            
            copyCmdList->ResourceBarrier( 1, &transitionDstBarrier );
        }

        for ( i32 i = 0; i < resourceCount; i++ ) {
            copyCmdList->CopyBufferRegion( buffer->resource[i], 0, uploadResource, 0, alignedSize );
        }
        uploadResource->Unmap( 0, &readRange );

        for ( i32 i = 0; i < resourceCount; i++ ) {
            D3D12_RESOURCE_BARRIER transitionDstBarrier2;
            transitionDstBarrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            transitionDstBarrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            transitionDstBarrier2.Transition.pResource = buffer->resource[i];
            transitionDstBarrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            transitionDstBarrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            transitionDstBarrier2.Transition.Subresource = 0;

            copyCmdList->ResourceBarrier( 1, &transitionDstBarrier2 );
        }

        copyCmdList->Close();
        renderContext->copyCmdQueue->ExecuteCommandLists( 1, reinterpret_cast< ID3D12CommandList** >( &copyCmdList ) );
        renderContext->copyCmdListUsageIndex[resourceIdx] = ( ++renderContext->copyCmdListUsageIndex[resourceIdx] % CMD_LIST_POOL_CAPACITY );

        // TODO Fence this to avoid memory leak but avoid race condition too...
        // Release temporary stuff
        // uploadResource->Release();
    }

    return buffer;
}

void RenderDevice::destroyBuffer( Buffer* buffer )
{
    // Static resource are single buffered
    if ( buffer->usage == RESOURCE_USAGE_STATIC
      || buffer->usage == RESOURCE_USAGE_STAGING ) {
        buffer->resource[0]->Release();
    } else {
        for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
            buffer->resource[i]->Release();
        }
    }

    // Remove the allocation request if the buffer is destroyed
    // This pointer should only be valid if the current buffer usage is dynamic and is using volatile buffers
    if ( buffer->allocationRequest != nullptr ) {
        dk::core::free( memoryAllocator, buffer->allocationRequest );
    }

    dk::core::free( memoryAllocator, buffer );
}

void RenderDevice::setDebugMarker( Buffer& buffer, const dkChar_t* objectName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    if ( buffer.usage == RESOURCE_USAGE_STATIC
      || buffer.usage == RESOURCE_USAGE_STAGING ) {
        buffer.resource[0]->SetName( objectName );
    } else {
        for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
            buffer.resource[i]->SetName( objectName );
        }
    }
#endif
}

void CommandList::bindVertexBuffer( const Buffer** buffers, const u32* offsets, const u32 bufferCount, const u32 startBindIndex )
{
    D3D12_VERTEX_BUFFER_VIEW vboView[MAX_VERTEX_BUFFER_BIND_COUNT];
    for ( u32 i = 0; i < bufferCount; i++ ) {
        vboView[i].BufferLocation = buffers[i]->resource[resourceFrameIndex]->GetGPUVirtualAddress();
        vboView[i].SizeInBytes = buffers[i]->size;
        vboView[i].StrideInBytes = buffers[i]->Stride;
    }

    nativeCommandList->graphicsCmdList->IASetVertexBuffers( startBindIndex, bufferCount, vboView );
}

void CommandList::bindIndiceBuffer( const Buffer* buffer, const bool use32bitsIndices )
{
    D3D12_INDEX_BUFFER_VIEW iboView;
    iboView.BufferLocation = buffer->resource[resourceFrameIndex]->GetGPUVirtualAddress();
    iboView.Format = ( use32bitsIndices ) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    iboView.SizeInBytes = buffer->size;

    nativeCommandList->graphicsCmdList->IASetIndexBuffer( &iboView );
}

void CommandList::updateBuffer( Buffer& buffer, const void* data, const size_t dataSize )
{
    void* bufferPointer = mapBuffer( buffer, static_cast<u32>( buffer.heapOffset ), static_cast<u32>( dataSize ) );
    if ( bufferPointer != nullptr ) {
        memcpy( bufferPointer, data, dataSize );
        unmapBuffer( buffer );
    }
}

void* CommandList::mapBuffer( Buffer& buffer, const u32 startOffsetInBytes, const u32 sizeInBytes )
{
    void* mappedMemory = nullptr;

    D3D12_RANGE bufferRange;
    bufferRange.Begin = startOffsetInBytes;
    bufferRange.End = sizeInBytes;

    buffer.memoryMappedRanges[buffer.memoryMappedRangeCount++] = bufferRange;

    HRESULT operationResult = buffer.resource[resourceFrameIndex]->Map( 0, &bufferRange, &mappedMemory );
    DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Buffer mapping FAILED! (error code 0x%x)" );

    return mappedMemory;
}

void CommandList::unmapBuffer( Buffer& buffer )
{
    buffer.resource[resourceFrameIndex]->Unmap( 0, &buffer.memoryMappedRanges[buffer.memoryMappedRangeCount--] );
}

void CommandList::transitionBuffer( Buffer& buffer, const eResourceState state )
{
    D3D12_RESOURCE_STATES nextState = RESOURCE_STATE_LUT[state];

    D3D12_RESOURCE_BARRIER transitionBarrier;
    transitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transitionBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transitionBarrier.Transition.pResource = buffer.resource[resourceFrameIndex];
    transitionBarrier.Transition.StateBefore = buffer.currentResourceState;
    transitionBarrier.Transition.StateAfter = nextState;
    transitionBarrier.Transition.Subresource = 0;

    nativeCommandList->graphicsCmdList->ResourceBarrier( 1, &transitionBarrier );

    buffer.currentResourceState = nextState;
}

void CommandList::copyBuffer( Buffer* sourceBuffer, Buffer* destBuffer )
{

}
#endif
