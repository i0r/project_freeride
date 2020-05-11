/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
struct ID3D12PipelineState;
struct ID3D12RootSignature;
struct ID3D10Blob;

#include <d3d12.h>

struct PipelineState
{
    enum class InternalResourceType {
        RESOURCE_TYPE_RENDER_TARGET_VIEW,
        RESOURCE_TYPE_DEPTH_STENCIL_VIEW,
        RESOURCE_TYPE_SHADER_RESOURCE_VIEW_IMAGE,
        RESOURCE_TYPE_SHADER_RESOURCE_VIEW_BUFFER,
        RESOURCE_TYPE_UNORDERED_ACCESS_VIEW_IMAGE,
        RESOURCE_TYPE_UNORDERED_ACCESS_VIEW_BUFFER,
        RESOURCE_TYPE_CBUFFER_VIEW
    };

    struct RTVDesc {
        DXGI_FORMAT         viewFormat;
        UINT                mipIndex;
        UINT                arrayIndex;
        bool                clearRenderTarget;
    };
    
    ID3D12PipelineState*        pso;
    ID3D12RootSignature*        rootSignature;
    ID3D10Blob*                 rootSignatureCache;

    D3D_PRIMITIVE_TOPOLOGY      primitiveTopology;

    i32                         resourceCount;
    InternalResourceType        resourcesType[PipelineStateDesc::MAX_RESOURCE_COUNT];

    RTVDesc                     renderTargetViewDesc[8]; 
    RTVDesc                     resourceViewDesc[PipelineStateDesc::MAX_RESOURCE_COUNT];
    RTVDesc                     depthStencilViewDesc;
    i32                         rtvCount;
    i32                         srvCount;
    bool                        hasDepthStencilView;

    FLOAT                       colorClearValue[4];
    FLOAT                       depthClearValue;
    u8                          stencilClearValue;

    PipelineState()
        : pso( nullptr )
        , rootSignature( nullptr )
        , rootSignatureCache( nullptr )
        , primitiveTopology( D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_UNDEFINED )
        , resourceCount( 0 )
        , rtvCount( 0 )
        , srvCount( 0 )
        , hasDepthStencilView( false )
        , depthClearValue( 0.0f )
        , stencilClearValue( 0xff )
    {
        memset( resourcesType, 0, sizeof( InternalResourceType ) * PipelineStateDesc::MAX_RESOURCE_COUNT );
        memset( renderTargetViewDesc, 0, sizeof( RTVDesc ) * 8 );
        memset( &depthStencilViewDesc, 0, sizeof( RTVDesc ) );
        memset( resourceViewDesc, 0, sizeof( RTVDesc ) * PipelineStateDesc::MAX_RESOURCE_COUNT );
        memset( colorClearValue, 0, sizeof( FLOAT ) * 4 );
    }
};
#endif
