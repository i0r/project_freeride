/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Core/ViewFormat.h>
#include <Maths/Helpers.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "PipelineState.h"

#include "Image.h"
#include "ResourceAllocationHelpers.h"

#include <d3d12.h>

Image* RenderDevice::createImage( const ImageDesc& description, const void* initialData, const size_t initialDataSize )
{
    D3D12_RESOURCE_DESC resourceDesc;
    if ( description.dimension == ImageDesc::DIMENSION_1D ) {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        resourceDesc.Width = description.width;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = description.arraySize;
    } else if ( description.dimension == ImageDesc::DIMENSION_2D ) {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Width = description.width;
        resourceDesc.Height = description.height;
        resourceDesc.DepthOrArraySize = description.arraySize;
    } else if ( description.dimension == ImageDesc::DIMENSION_3D ) {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        resourceDesc.Width = description.width;
        resourceDesc.Height = description.height;
        resourceDesc.DepthOrArraySize = description.depth * description.arraySize;
    }

    DXGI_FORMAT nativeFormat = static_cast< DXGI_FORMAT >( description.format );
    resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    resourceDesc.MipLevels = description.mipCount;
    resourceDesc.Format = nativeFormat;
    resourceDesc.SampleDesc.Count = description.samplerCount;
    resourceDesc.SampleDesc.Quality = ( description.samplerCount > 1 ) ? DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN : 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    resourceDesc.Flags = GetNativeResourceFlags( description.bindFlags );
    
    ID3D12Device* device = renderContext->device;
    D3D12_RESOURCE_STATES stateFlags = ( initialData == nullptr ) ? GetResourceStateFlags( description.bindFlags ) : D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
    
    Image* image = dk::core::allocate<Image>( memoryAllocator );
    image->width = description.width;
    image->height = description.height;
    image->arraySize = description.arraySize;
    image->samplerCount = description.samplerCount;
    image->mipCount = description.mipCount;
    image->usage = description.usage;
    image->dimension = description.dimension;
    image->defaultFormat = nativeFormat;

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        image->currentResourceState[i] = stateFlags;
    }

    D3D12_RESOURCE_ALLOCATION_INFO allocInfos;
    HRESULT operationResult = S_OK;
    switch ( description.usage ) {
    case RESOURCE_USAGE_STATIC: {
        operationResult = device->CreatePlacedResource(
            renderContext->staticImageHeap,
            renderContext->heapOffset,
            &resourceDesc,  
            stateFlags,
            nullptr,
            __uuidof( ID3D12Resource ),
            reinterpret_cast< void** >( &image->resource[0] )
        );
        DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Image creation FAILED! (error code: 0x%x)", operationResult );

        // Static images are single-buffered; copy the original handle for convenience in the other slots
        for ( i32 i = 1; i < PENDING_FRAME_COUNT; i++ ) {
            image->resource[i] = image->resource[0];
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
            reinterpret_cast< void** >( &image->resource[0] )
        );
        DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Image creation FAILED! (error code: 0x%x)", operationResult );

        for ( i32 i = 1; i < PENDING_FRAME_COUNT; i++ ) {
            image->resource[i] = image->resource[0];
        }
    } break;
    case RESOURCE_USAGE_DEFAULT: {
        D3D12_CLEAR_VALUE optimizedClearValue;
        optimizedClearValue.Color[0] = 0.0f;
        optimizedClearValue.Color[1] = 0.0f;
        optimizedClearValue.Color[2] = 0.0f;
        optimizedClearValue.Color[3] = 0.0f;
        optimizedClearValue.Format = nativeFormat;

        // Check if optimized clear is available (based on resource's bind flagset)
        D3D12_CLEAR_VALUE* pOptimizedClear = ( description.bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ) ? &optimizedClearValue : nullptr;

        bool isRtvOrDsv = ( description.bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW || description.bindFlags & RESOURCE_BIND_DEPTH_STENCIL );
        if ( description.bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW && !isRtvOrDsv ) {
            for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
                operationResult = device->CreatePlacedResource(
                    renderContext->dynamicUavImageHeap,
                    renderContext->dynamicUavImageHeapPerFrameCapacity * i + renderContext->dynamicUavImageHeapOffset,
                    &resourceDesc,
                    stateFlags,
                    pOptimizedClear,
                    __uuidof( ID3D12Resource ),
                    reinterpret_cast< void** >( &image->resource[i] )
                );

                // TODO Should we take alignment in account? Or assume every allocation will be memory aligned by default?
                allocInfos = device->GetResourceAllocationInfo( 0, 1, &resourceDesc );
                renderContext->dynamicUavImageHeapOffset += allocInfos.SizeInBytes;

                DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Image creation FAILED! (error code: 0x%x)", operationResult );
            }
        } else if ( isRtvOrDsv ) {
            for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
                operationResult = device->CreatePlacedResource(
                    renderContext->dynamicImageHeap,
                    renderContext->dynamicImageHeapPerFrameCapacity * i + renderContext->dynamicImageHeapOffset,
                    &resourceDesc,
                    stateFlags,
                    pOptimizedClear,
                    __uuidof( ID3D12Resource ),
                    reinterpret_cast< void** >( &image->resource[i] )
                );

                // TODO Should we take alignment in account? Or assume every allocation will be memory aligned by default?
                allocInfos = device->GetResourceAllocationInfo( 0, 1, &resourceDesc );
                renderContext->dynamicImageHeapOffset += allocInfos.SizeInBytes;

                DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Image creation FAILED! (error code: 0x%x)", operationResult );
            }
        } else {
            DUSK_RAISE_FATAL_ERROR( false, "Invalid bindFlags combination! (An Image with a usage DEFAULT must be writable using a RTV or a UAV)" );
        }
    } break;
    }

    if ( initialData != nullptr ) {
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

        // Allocate a cmd list on the copy command queue in order to perform texel copy
        size_t resourceIdx = frameIndex % PENDING_FRAME_COUNT;
        size_t cmdListIdx = renderContext->copyCmdListUsageIndex[resourceIdx];
        ID3D12CommandAllocator* cmdListAllocator = renderContext->copyCmdAllocator[resourceIdx][cmdListIdx];
        ID3D12GraphicsCommandList* copyCmdList = renderContext->copyCmdLists[resourceIdx][cmdListIdx];
        cmdListAllocator->Reset();
        copyCmdList->Reset( cmdListAllocator, nullptr );

        u32 subresourceCount = description.arraySize * description.mipCount;

        // Transition each mip before doing the copy work
        D3D12_RESOURCE_BARRIER transitionDstBarrier;
        transitionDstBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transitionDstBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        transitionDstBarrier.Transition.pResource = image->resource[0];
        transitionDstBarrier.Transition.StateBefore = stateFlags;
        transitionDstBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        transitionDstBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        copyCmdList->ResourceBarrier( 1, &transitionDstBarrier );

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[100];
        UINT64 rowSizeInBytes[100], uploadSize;
        UINT numRows[100];
        device->GetCopyableFootprints( &resourceDesc, 0, subresourceCount, 0, footprints, numRows, rowSizeInBytes, &uploadSize );

        for ( u32 mipIdx = 0; mipIdx < subresourceCount; mipIdx++ ) {
            for ( UINT row = 0; row < numRows[mipIdx]; row++ ) {
                // TODO Investigate random crashes? (pretty sure there is a buffer overrun somewhere...)
                memcpy( (BYTE*)pData + footprints[mipIdx].Offset + footprints[mipIdx].Footprint.RowPitch * row, ( BYTE* )initialData + footprints[mipIdx].Offset + rowSizeInBytes[mipIdx] * row, rowSizeInBytes[mipIdx] );
            }
            
            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = footprints[mipIdx];

            D3D12_TEXTURE_COPY_LOCATION srcLocation;
            srcLocation.pResource = uploadResource;
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLocation.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION dstLocation;
            dstLocation.pResource = image->resource[0];
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLocation.SubresourceIndex = mipIdx;

            copyCmdList->CopyTextureRegion( &dstLocation, 0, 0, 0, &srcLocation, nullptr );
        }

        uploadResource->Unmap( 0, &readRange );

        // Transition each mip to their proper initial resource state
        stateFlags = GetResourceStateFlags( description.bindFlags );

        D3D12_RESOURCE_BARRIER endTansitionDstBarrier;
        endTansitionDstBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        endTansitionDstBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        endTansitionDstBarrier.Transition.pResource = image->resource[0];
        endTansitionDstBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        endTansitionDstBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        endTansitionDstBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        copyCmdList->ResourceBarrier( 1, &endTansitionDstBarrier );

        copyCmdList->Close();
        renderContext->copyCmdQueue->ExecuteCommandLists( 1, reinterpret_cast<ID3D12CommandList**>( &copyCmdList ) );

        renderContext->copyCmdListUsageIndex[resourceIdx] = ( ++renderContext->copyCmdListUsageIndex[resourceIdx] % CMD_LIST_POOL_CAPACITY );

        // Release temporary stuff
        // uploadResource->Release();
    }

    return image;
}

void RenderDevice::destroyImage( Image* image )
{
    // Static resources (e.g. material textures, shared LUT, etc.) are single buffered
    if ( image->usage == RESOURCE_USAGE_STATIC
      || image->usage == RESOURCE_USAGE_STAGING ) {
        image->resource[0]->Release();
    } else {
        for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
            image->resource[i]->Release();
        }
    }

    dk::core::free( memoryAllocator, image );
}

void RenderDevice::setDebugMarker( Image& image, const dkChar_t* objectName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    if ( image.usage == RESOURCE_USAGE_STATIC
      || image.usage == RESOURCE_USAGE_STAGING ) {
        image.resource[0]->SetName( objectName );
    } else {
        for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
            image.resource[i]->SetName( objectName );
        }
    }
#endif
}

void CommandList::transitionImage( Image& image, const eResourceState state, const u32 mipIndex, const TransitionType transitionType )
{
    D3D12_RESOURCE_STATES nextState = RESOURCE_STATE_LUT[state];

    D3D12_RESOURCE_BARRIER transitionBarrier;
    transitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transitionBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transitionBarrier.Transition.pResource = image.resource[resourceFrameIndex];
    transitionBarrier.Transition.StateBefore = image.currentResourceState[resourceFrameIndex];
    transitionBarrier.Transition.StateAfter = nextState;
    transitionBarrier.Transition.Subresource = mipIndex;

    nativeCommandList->graphicsCmdList->ResourceBarrier( 1, &transitionBarrier );

    image.currentResourceState[resourceFrameIndex] = nextState;
}

void CommandList::insertComputeBarrier( Image& image )
{
    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = image.resource[resourceFrameIndex];

    nativeCommandList->graphicsCmdList->ResourceBarrier( 1, &uavBarrier );
}

static D3D12_CPU_DESCRIPTOR_HANDLE CreateRenderTargetView( RenderContext* renderContext, const u32 internalFrameIdx, const PipelineState::RTVDesc& psoRtvDesc, Image* renderBuffer )
{
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = ( psoRtvDesc.viewFormat == DXGI_FORMAT_UNKNOWN ) ? renderBuffer->defaultFormat : psoRtvDesc.viewFormat;

    switch ( renderBuffer->dimension ) {
    case ImageDesc::DIMENSION_1D: {
        if ( renderBuffer->arraySize > 1 ) {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            rtvDesc.Texture1DArray.MipSlice = psoRtvDesc.mipIndex;
            rtvDesc.Texture1DArray.ArraySize = 1;
            rtvDesc.Texture1DArray.FirstArraySlice = psoRtvDesc.arrayIndex;
        } else {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            rtvDesc.Texture1D.MipSlice = psoRtvDesc.mipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_2D: {
        if ( renderBuffer->samplerCount > 1 ) {
            if ( renderBuffer->arraySize > 1 ) {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                rtvDesc.Texture2DMSArray.ArraySize = 1;
                rtvDesc.Texture2DMSArray.FirstArraySlice = psoRtvDesc.arrayIndex;
            } else {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
        } else {
            if ( renderBuffer->arraySize > 1 ) {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = psoRtvDesc.mipIndex;
                rtvDesc.Texture2DArray.ArraySize = 1;
                rtvDesc.Texture2DArray.PlaneSlice = 0;
                rtvDesc.Texture2DArray.FirstArraySlice = psoRtvDesc.arrayIndex;
            } else {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = psoRtvDesc.mipIndex;
                rtvDesc.Texture2D.PlaneSlice = 0;
            }
        }
    } break;
    case ImageDesc::DIMENSION_3D: {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice = psoRtvDesc.mipIndex;
        rtvDesc.Texture3D.FirstWSlice = psoRtvDesc.arrayIndex;
        rtvDesc.Texture3D.WSize = 1;
    } break;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderContext->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += renderContext->rtvDescriptorHeapOffset;
    renderContext->rtvDescriptorHeapOffset += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

    renderContext->device->CreateRenderTargetView( renderBuffer->resource[internalFrameIdx], &rtvDesc, rtvHandle );

    return rtvHandle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE CreateDepthStencilView( RenderContext* renderContext, const u32 internalFrameIdx, const PipelineState::RTVDesc& psoDsvDesc, Image* renderBuffer )
{
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format = ( psoDsvDesc.viewFormat == DXGI_FORMAT_UNKNOWN ) ? renderBuffer->defaultFormat : psoDsvDesc.viewFormat;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    switch ( renderBuffer->dimension ) {
    case ImageDesc::DIMENSION_1D: {
        if ( renderBuffer->arraySize > 1 ) {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            dsvDesc.Texture1DArray.MipSlice = psoDsvDesc.mipIndex;
            dsvDesc.Texture1DArray.ArraySize = 1;
            dsvDesc.Texture1DArray.FirstArraySlice = psoDsvDesc.arrayIndex;
        } else {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            dsvDesc.Texture1D.MipSlice = psoDsvDesc.mipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_2D: {
        if ( renderBuffer->samplerCount > 1 ) {
            if ( renderBuffer->arraySize > 1 ) {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                dsvDesc.Texture2DMSArray.ArraySize = 1;
                dsvDesc.Texture2DMSArray.FirstArraySlice = psoDsvDesc.arrayIndex;
            } else {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
        } else {
            if ( renderBuffer->arraySize > 1 ) {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.MipSlice = psoDsvDesc.mipIndex;
                dsvDesc.Texture2DArray.ArraySize = 1;
                dsvDesc.Texture2DArray.FirstArraySlice = psoDsvDesc.arrayIndex;
            } else {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = psoDsvDesc.mipIndex;
            }
        }
    } break;
    default:
        DUSK_DEV_ASSERT( false, "Invalid API usage! (a depth/stencil image can't be 3D)" );
        break;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = renderContext->dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    dsvHandle.ptr += renderContext->dsvDescriptorHeapOffset;

    renderContext->dsvDescriptorHeapOffset += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
    renderContext->device->CreateDepthStencilView( renderBuffer->resource[internalFrameIdx], &dsvDesc, dsvHandle );
       
    return dsvHandle;
}

void CommandList::setupFramebuffer( FramebufferAttachment* renderTargetViews, FramebufferAttachment depthStencilView )
{
    const PipelineState& pipelineState = *nativeCommandList->BindedPipelineState;
    bool hasDsv = pipelineState.hasDepthStencilView;
    bool shouldClearDsv = false;
    UINT renderTargetCount = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8];
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_CPU_DESCRIPTOR_HANDLE clearRtvs[8];
    D3D12_RECT clearRectangles[8];
    D3D12_RECT clearDepthRectangle;
    UINT clearRenderTargetCount = 0;

    ID3D12GraphicsCommandList* cmdList = nativeCommandList->graphicsCmdList;

    u32 rtvCount = pipelineState.rtvCount;
    for ( u32 i = 0; i < rtvCount; i++ ) {
        Image* renderBuffer = renderTargetViews[i].ImageAttachment;
        const PipelineState::RTVDesc& psoRtvDesc = pipelineState.renderTargetViewDesc[renderTargetCount];

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CreateRenderTargetView( nativeCommandList->renderContext, resourceFrameIndex, psoRtvDesc, renderBuffer );
        rtvs[renderTargetCount++] = rtvHandle;

        // Pre-transition render target to the correct state (no idea if this is the right place to do it; it might need to be done at higher level?)
        if ( renderBuffer->currentResourceState[resourceFrameIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET ) {
            transitionImage( *renderBuffer, RESOURCE_STATE_RENDER_TARGET, psoRtvDesc.mipIndex );
        }

        // TODO Find a clean way to make the optimized clean value match the resource list clean value
        if ( psoRtvDesc.clearRenderTarget ) {
            D3D12_RECT& clearRectangle = clearRectangles[clearRenderTargetCount];
            clearRectangle.top = 0;
            clearRectangle.left = 0;
            clearRectangle.right = renderBuffer->width;
            clearRectangle.bottom = renderBuffer->height;

            clearRtvs[clearRenderTargetCount] = rtvHandle;
            clearRenderTargetCount++;
        }
    }

    // Create DSV if a zbuffer is binded
    if ( pipelineState.hasDepthStencilView ) {
        const PipelineState::RTVDesc& psoDsvDesc = pipelineState.depthStencilViewDesc;

        Image* dsvAttachment = depthStencilView.ImageAttachment;
        dsv = CreateDepthStencilView( nativeCommandList->renderContext, resourceFrameIndex, psoDsvDesc, dsvAttachment );

        // Pre-transition render target to the correct state (no idea if this is the right place to do it; it might need to be done at higher level?)
        if ( dsvAttachment->currentResourceState[resourceFrameIndex] != D3D12_RESOURCE_STATE_DEPTH_WRITE ) {
            transitionImage( *dsvAttachment, RESOURCE_STATE_DEPTH_WRITE, psoDsvDesc.mipIndex );
        }

        if ( psoDsvDesc.clearRenderTarget ) {
            // TODO Find a clean way to make the optimized clean value match the resource list clean value
            D3D12_RECT& clearRectangle = clearDepthRectangle;
            clearRectangle.top = 0;
            clearRectangle.left = 0;
            clearRectangle.right = dsvAttachment->width;
            clearRectangle.bottom = dsvAttachment->height;

            shouldClearDsv = true;
        }
    }

    // Initialize / Clear render buffers
    for ( u32 i = 0; i < clearRenderTargetCount; i++ ) {
        cmdList->ClearRenderTargetView( clearRtvs[i], pipelineState.colorClearValue, 1, &clearRectangles[i] );
    }

   if ( shouldClearDsv ) {
        cmdList->ClearDepthStencilView( dsv, D3D12_CLEAR_FLAG_DEPTH, pipelineState.depthClearValue, pipelineState.stencilClearValue, 1, &clearDepthRectangle );
    }

    cmdList->OMSetRenderTargets( renderTargetCount, rtvs, FALSE, ( hasDsv ) ? &dsv : nullptr );
}

void RenderDevice::createImageView( Image& image, const ImageViewDesc& viewDescription, const u32 creationFlags )
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
