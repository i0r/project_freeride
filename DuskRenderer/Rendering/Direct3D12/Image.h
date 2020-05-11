/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
enum DXGI_FORMAT;
enum D3D12_RESOURCE_STATES;
struct ID3D12Resource;

struct Image
{
    ID3D12Resource*                  resource[RenderDevice::PENDING_FRAME_COUNT];
    UINT                             width;
    UINT                             height;
    UINT                             arraySize;
    UINT                             samplerCount;
    u32                              mipCount;
    D3D12_RESOURCE_STATES            currentResourceState[RenderDevice::PENDING_FRAME_COUNT];
    eResourceUsage                   usage;
    decltype( ImageDesc::dimension ) dimension;
    DXGI_FORMAT                      defaultFormat;

    Image()
        : width( 0 )
        , height( 0 )
        , arraySize( 0 )
        , samplerCount( 0 )
        , mipCount( 0 )
        , usage( RESOURCE_USAGE_DEFAULT )
        , dimension( ImageDesc::DIMENSION_UNKNOWN )
        , defaultFormat( static_cast< DXGI_FORMAT >( 0 ) )
    {
        memset( resource, 0, sizeof( ID3D12Resource* ) * RenderDevice::PENDING_FRAME_COUNT );
        memset( currentResourceState, 0, sizeof( D3D12_RESOURCE_STATES )  * RenderDevice::PENDING_FRAME_COUNT );
    }
};
#endif
