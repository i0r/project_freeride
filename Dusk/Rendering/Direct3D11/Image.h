/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D11
struct ID3D11Texture1D;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
enum DXGI_FORMAT;

#include <Rendering/RenderDevice.h> // used by ImageDesc
#include <unordered_map>

struct Image
{
    union {
        ID3D11Texture1D*			texture1D;
        ID3D11Texture2D*			texture2D;
        ID3D11Texture3D*			texture3D;
        ID3D11Resource*             textureResource;
    };

    ID3D11ShaderResourceView*       DefaultShaderResourceView;
    ID3D11UnorderedAccessView*      DefaultUnorderedAccessView;

    union {
        ID3D11RenderTargetView*     DefaultRenderTargetView;
        ID3D11DepthStencilView*     DefaultDepthStencilView;
    };

    // Current UAV register index (=0xffffffff if the resource is not binded to the device).
    i32                             UAVRegisterIndex;

    // Current UAV register index (=0xffffffff if the resource is not binded to the device).
    i32                             PSUAVRegisterIndex;

    struct FBOAttachment
	{
		union
		{
			ID3D11RenderTargetView* RenderTargetView;
			ID3D11DepthStencilView* DepthStencilView;
		};
    };
	std::unordered_map<u64, FBOAttachment> RTVs;

    std::unordered_map<u64, ID3D11ShaderResourceView*> SRVs;

    std::unordered_map<u64, ID3D11UnorderedAccessView*> UAVs;

    // Current SRV register index (=0xffffffff if the resource is not binded to the device).
    i32                             SRVRegisterIndex[eShaderStage::SHADER_STAGE_COUNT];

    // True if the image is currently binded to the Framebuffer.
    u8                              IsBindedToFBO : 1;

    // Descriptor used for this image.
    ImageDesc                       Description;

    Image()
        : textureResource( nullptr )
        , DefaultShaderResourceView( nullptr )
        , DefaultUnorderedAccessView( nullptr )
        , DefaultRenderTargetView( nullptr )
        , UAVRegisterIndex( ~0 )
        , PSUAVRegisterIndex( ~0 )
        , IsBindedToFBO( false )
        , Description()
    {
        memset( SRVRegisterIndex, 0xff, sizeof( i32 )* eShaderStage::SHADER_STAGE_COUNT );
    }

    ~Image()
    {

    }
};

void SetupFramebuffer_Replay( RenderContext* renderContext, FramebufferAttachment* renderTargetViews, FramebufferAttachment depthStencilView );
#endif
