/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
#include <d3d12.h>

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