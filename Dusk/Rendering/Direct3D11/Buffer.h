/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D11
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

struct Buffer
{
    // Buffer handle allocated by D3D11.
    ID3D11Buffer*               BufferObject;
    
    // Buffer stride (copied from BufferDesc).
    u32                         Stride;
    
    // Buffer bind flags (copied from BufferDesc).
    u32                         BindFlags;

    // Buffer default SRV (if BindFlags allow SRV bind).
    ID3D11ShaderResourceView*   DefaultShaderResourceView;

    // Buffer default UAV (if BindFlags allow UAV bind).
    ID3D11UnorderedAccessView*  DefaultUnorderedAccessView;

    // Current UAV register index (=0xffffffff if the resource is not binded to the device).
    i32                         UAVRegisterIndex;

    // Current SRV register index (=0xffffffff if the resource is not binded to the device).
    i32                         SRVRegisterIndex[eShaderStage::SHADER_STAGE_COUNT];

    // Current Cbuffer register index (=0xffffffff if the resource is not binded to the device).
    i32                         CbufferRegisterIndex[eShaderStage::SHADER_STAGE_COUNT];

    Buffer()
        : BufferObject( nullptr )
        , Stride( 0 )
        , BindFlags( 0 )
        , DefaultShaderResourceView( nullptr )
        , DefaultUnorderedAccessView( nullptr )
        , UAVRegisterIndex( ~0 )
    {
        memset( SRVRegisterIndex, 0xff, sizeof( i32 ) * eShaderStage::SHADER_STAGE_COUNT );
        memset( CbufferRegisterIndex, 0xff, sizeof( i32 ) * eShaderStage::SHADER_STAGE_COUNT );
    }
};
#endif
