/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
#include <d3d12.h>

namespace
{
    static constexpr size_t RESOURCE_ALLOC_ALIGNMENT = 65536;
}

DUSK_INLINE D3D12_RESOURCE_FLAGS GetNativeResourceFlags( const u32 bindFlags )
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if ( bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if ( bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        // Might save bandwidth according to Microsoft documentation
        // D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL must be toggled to use D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
        if ( !( bindFlags & RESOURCE_BIND_SHADER_RESOURCE ) ) {
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
    }
    if ( bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    return flags;
}

DUSK_INLINE D3D12_RESOURCE_STATES GetResourceStateFlags( const u32 bindFlags )
{
    D3D12_RESOURCE_STATES flags = D3D12_RESOURCE_STATE_COMMON;

    if ( ( bindFlags & RESOURCE_BIND_VERTEX_BUFFER )
        || ( bindFlags & RESOURCE_BIND_CONSTANT_BUFFER ) ) {
        return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }

    if ( bindFlags & RESOURCE_BIND_INDICE_BUFFER ) {
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }

    if ( bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    if ( bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) {
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    if ( bindFlags & RESOURCE_BIND_SHADER_RESOURCE ) {
        return ( D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
    }

    if ( bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ) {
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    if ( bindFlags & RESOURCE_BIND_INDIRECT_ARGUMENTS ) {
        return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }

    return D3D12_RESOURCE_STATE_COMMON;
}

// Helper function to create a resource on a given heap.
DUSK_INLINE HRESULT CreatePlacedResource( 
    ID3D12Device* device, 
    ID3D12Heap* heap, 
    const D3D12_RESOURCE_DESC& resourceDesc, 
    const D3D12_RESOURCE_STATES stateFlags, 
    const u64 heapOffset,
    ID3D12Resource** resourceToCreate,
    const D3D12_CLEAR_VALUE* optimizedClear = nullptr )
{
    HRESULT operationResult = device->CreatePlacedResource(
        heap,
        heapOffset,
        &resourceDesc,
        stateFlags,
        optimizedClear,
        __uuidof( ID3D12Resource ),
        reinterpret_cast< void** >( resourceToCreate )
    );

    DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "ID3D12Resource creation failed! (error code: 0x%x)", operationResult );

    return operationResult;
}

// Realign a given heap offset to satisfy alignment restrictions from D3D12. Note that this resource should be used
// for static resource allocation (realignment for dynamic resources allocation will lead to heap fragmentation).
// Dynamic resources allocation should either use volatile memory allocation or use proper allocation scheme (managed at
// a higher level).
DUSK_INLINE u64 RealignHeapOffset( const D3D12_RESOURCE_ALLOCATION_INFO& allocInfos, const u64 heapOffset, const size_t alignmentInBytes = RESOURCE_ALLOC_ALIGNMENT )
{
    // Realign heap offset.
    size_t alignedOffset = ( allocInfos.SizeInBytes < alignmentInBytes )
        ? ( heapOffset + alignmentInBytes )
        : heapOffset + ( allocInfos.SizeInBytes + ( alignmentInBytes - ( allocInfos.SizeInBytes % alignmentInBytes ) ) );

    return alignedOffset;
}

static constexpr D3D12_RESOURCE_STATES RESOURCE_STATE_LUT[16] = {
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    D3D12_RESOURCE_STATE_DEPTH_READ,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_COPY_SOURCE,
    D3D12_RESOURCE_STATE_RESOLVE_DEST,
    D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_INDEX_BUFFER
};
#endif
