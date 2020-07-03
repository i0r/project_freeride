/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D11
#include <Rendering/RenderDevice.h>
#include <d3d11.h>

#include "Buffer.h"
#include "Image.h"

static UINT GetNativeBindFlags( const u32 bindFlags )
{
    UINT nativeBindFlags = 0;

    if ( bindFlags & RESOURCE_BIND_CONSTANT_BUFFER ) {
        nativeBindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    }

    if ( bindFlags & RESOURCE_BIND_VERTEX_BUFFER ) {
        nativeBindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }

    if ( bindFlags & RESOURCE_BIND_INDICE_BUFFER ) {
        nativeBindFlags |= D3D11_BIND_INDEX_BUFFER;
    }

    if ( bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
        nativeBindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if ( bindFlags & RESOURCE_BIND_SHADER_RESOURCE ) {
        nativeBindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    if ( bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ) {
        nativeBindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    if ( bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) {
        nativeBindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }

    return nativeBindFlags;
}

static UINT GetCPUUsageFlags( const eResourceUsage usage )
{
    switch ( usage ) {
    case RESOURCE_USAGE_STAGING:
        return D3D11_CPU_ACCESS_READ;
    case RESOURCE_USAGE_DYNAMIC:
        return D3D11_CPU_ACCESS_WRITE;
    default:
        return 0;
    }
}

// NOTE miscFlags should only be used by Texture/Render Targets (matches the
// same field in ImageDescription struct)
static UINT GetMiscFlags( const u32 bindFlags, const u32 miscFlags = 0 )
{
    UINT nativeMiscFlags = 0;

    if ( bindFlags & RESOURCE_BIND_STRUCTURED_BUFFER ) {
        nativeMiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    }

    if ( bindFlags & RESOURCE_BIND_INDIRECT_ARGUMENTS ) {
        nativeMiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }

    if ( bindFlags & RESOURCE_BIND_RAW ) {
        nativeMiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }

    if ( miscFlags & ImageDesc::ENABLE_HARDWARE_MIP_GENERATION ) {
        nativeMiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    if ( miscFlags & ImageDesc::IS_CUBE_MAP ) {
        nativeMiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    return nativeMiscFlags;
}

static D3D11_USAGE GetNativeUsage( const eResourceUsage usage )
{
    switch ( usage ) {
    case RESOURCE_USAGE_DEFAULT:
        return D3D11_USAGE_DEFAULT;
    case RESOURCE_USAGE_STATIC:
        return D3D11_USAGE_IMMUTABLE;
    case RESOURCE_USAGE_DYNAMIC:
        return D3D11_USAGE_DYNAMIC;
    case RESOURCE_USAGE_STAGING:
        return D3D11_USAGE_STAGING;
    default:
        DUSK_ASSERT( false, "Not implemented!" );
    }

    return D3D11_USAGE_DEFAULT;
}

static ID3D11ShaderResourceView* CreateImageShaderResourceView( ID3D11Device* device, const Image& image, const ImageViewDesc& viewDesc )
{
    const DXGI_FORMAT viewFormat = static_cast< DXGI_FORMAT >( viewDesc.ViewFormat );

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = viewFormat;

    switch ( shaderResourceViewDesc.Format ) {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        shaderResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
    }

    const bool isArrayView = ( viewDesc.ImageCount > 1 );
    const UINT mipCount = ( viewDesc.MipCount <= 0 ) ? -1 : viewDesc.MipCount;
    const UINT imgCount = ( viewDesc.ImageCount <= 0 ) ? -1 : viewDesc.ImageCount;

    switch ( image.Description.dimension ) {
    case ImageDesc::DIMENSION_1D:
    {
        if ( isArrayView ) {
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;

            shaderResourceViewDesc.Texture1DArray.ArraySize = imgCount;
            shaderResourceViewDesc.Texture1DArray.FirstArraySlice = viewDesc.StartImageIndex;

            shaderResourceViewDesc.Texture1DArray.MipLevels = mipCount;
            shaderResourceViewDesc.Texture1DArray.MostDetailedMip = viewDesc.StartMipIndex;
        } else {
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;

            shaderResourceViewDesc.Texture1D.MipLevels = mipCount;
            shaderResourceViewDesc.Texture1D.MostDetailedMip = viewDesc.StartMipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_2D:
    {
        const bool isMultisampled = ( image.Description.samplerCount > 1 );
        const bool isCubemap = ( image.Description.miscFlags & ImageDesc::IS_CUBE_MAP );

        if ( isMultisampled ) {
            // TEXTURE2DMS
            if ( isArrayView ) {
                shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;

                shaderResourceViewDesc.Texture2DMSArray.ArraySize = imgCount;
                shaderResourceViewDesc.Texture2DMSArray.FirstArraySlice = viewDesc.StartImageIndex;
            } else {
                shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
            }
        } else if ( isCubemap && imgCount == 0 ) {
            // TEXTURECUBE
            if ( isArrayView ) {
                shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;

                shaderResourceViewDesc.TextureCubeArray.MipLevels = mipCount;
                shaderResourceViewDesc.TextureCubeArray.MostDetailedMip = viewDesc.StartMipIndex;
                shaderResourceViewDesc.TextureCubeArray.First2DArrayFace = 0;
                shaderResourceViewDesc.TextureCubeArray.NumCubes = ( UINT )( imgCount / image.Description.depth );
            } else {
                shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;

                shaderResourceViewDesc.TextureCube.MipLevels = mipCount;
                shaderResourceViewDesc.TextureCube.MostDetailedMip = viewDesc.StartMipIndex;
            }
        } else {
            // TEXTURE2D
            if ( isArrayView || isCubemap ) {
                shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;

                shaderResourceViewDesc.Texture2DArray.ArraySize = imgCount;
                shaderResourceViewDesc.Texture2DArray.FirstArraySlice = viewDesc.StartImageIndex;

                shaderResourceViewDesc.Texture2DArray.MipLevels = mipCount;
                shaderResourceViewDesc.Texture2DArray.MostDetailedMip = viewDesc.StartMipIndex;
            } else {
                shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

                shaderResourceViewDesc.Texture2D.MipLevels = mipCount;
                shaderResourceViewDesc.Texture2D.MostDetailedMip = viewDesc.StartMipIndex;
            }
        }
    } break;
    case ImageDesc::DIMENSION_3D:
    {
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        shaderResourceViewDesc.Texture3D.MipLevels = mipCount;
        shaderResourceViewDesc.Texture3D.MostDetailedMip = viewDesc.StartMipIndex;
    } break;
    }

    ID3D11ShaderResourceView* shaderResourceView = nullptr;
    HRESULT operationResult = device->CreateShaderResourceView( image.textureResource, &shaderResourceViewDesc, &shaderResourceView );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "ShaderResourceView creation failed (error code: 0x%x)!", operationResult );

    return shaderResourceView;
}

static ID3D11UnorderedAccessView* CreateImageUnorderedAccessView( ID3D11Device* device, const Image& image, const ImageViewDesc& viewDesc )
{
    const DXGI_FORMAT viewFormat = static_cast< DXGI_FORMAT >( viewDesc.ViewFormat );

    D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc;
    unorderedAccessViewDesc.Format = viewFormat;
    
    const bool isArrayView = ( viewDesc.ImageCount > 1 );
    const UINT mipCount = ( viewDesc.MipCount <= 0 ) ? -1 : viewDesc.MipCount;
    const UINT imgCount = ( viewDesc.ImageCount <= 0 ) ? -1 : viewDesc.ImageCount;

    switch ( image.Description.dimension ) {
    case ImageDesc::DIMENSION_1D:
    {
        if ( isArrayView ) {
            unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;

            unorderedAccessViewDesc.Texture1DArray.ArraySize = imgCount;
            unorderedAccessViewDesc.Texture1DArray.FirstArraySlice = viewDesc.StartImageIndex;

            unorderedAccessViewDesc.Texture1DArray.MipSlice = viewDesc.StartMipIndex;
        } else {
            unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;

            unorderedAccessViewDesc.Texture1D.MipSlice = viewDesc.StartMipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_2D:
    {
        const bool isMultisampled = ( image.Description.samplerCount > 1 );
        const bool isCubemap = ( image.Description.miscFlags & ImageDesc::IS_CUBE_MAP );

        if ( isMultisampled || ( isCubemap && imgCount == 0 ) ) {
            DUSK_LOG_ERROR( "Failed to create UAV: the image provided is not suitable for UAV creation (IsMultisampled: %i == 1 or IsCubemap: %i == 1)", isMultisampled, isCubemap );
            break;
        }

        if ( isArrayView || isCubemap ) {
            unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;

            unorderedAccessViewDesc.Texture2DArray.ArraySize = imgCount;
            unorderedAccessViewDesc.Texture2DArray.FirstArraySlice = viewDesc.StartImageIndex;

            unorderedAccessViewDesc.Texture2DArray.MipSlice = viewDesc.StartMipIndex;
        } else {
            unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

            unorderedAccessViewDesc.Texture2D.MipSlice = viewDesc.StartMipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_3D:
    {
        unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        unorderedAccessViewDesc.Texture3D.FirstWSlice = viewDesc.StartImageIndex;
        unorderedAccessViewDesc.Texture3D.WSize = imgCount;

        unorderedAccessViewDesc.Texture3D.MipSlice = viewDesc.StartMipIndex;
    } break;
    }

    ID3D11UnorderedAccessView* unorderedAccessView = nullptr;
    HRESULT operationResult = device->CreateUnorderedAccessView( image.textureResource, &unorderedAccessViewDesc, &unorderedAccessView );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "UnorderedAccessView creation failed (error code: 0x%x)!", operationResult );

    return unorderedAccessView;
}

static ID3D11RenderTargetView* CreateImageRenderTargetView( ID3D11Device* device, const Image& image, const ImageViewDesc& viewDesc )
{
    const DXGI_FORMAT viewFormat = static_cast< DXGI_FORMAT >( viewDesc.ViewFormat );

    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    renderTargetViewDesc.Format = viewFormat;

    const bool isArrayView = ( viewDesc.ImageCount > 1 );
    const UINT mipCount = ( viewDesc.MipCount <= 0 ) ? -1 : viewDesc.MipCount;
    const UINT imgCount = ( viewDesc.ImageCount <= 0 ) ? -1 : viewDesc.ImageCount;

    switch ( image.Description.dimension ) {
    case ImageDesc::DIMENSION_1D:
    {
        if ( isArrayView ) {
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;

            renderTargetViewDesc.Texture1DArray.ArraySize = imgCount;
            renderTargetViewDesc.Texture1DArray.FirstArraySlice = viewDesc.StartImageIndex;

            renderTargetViewDesc.Texture1DArray.MipSlice = viewDesc.StartMipIndex;
        } else {
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;

            renderTargetViewDesc.Texture1D.MipSlice = viewDesc.StartMipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_2D:
    {
        const bool isMultisampled = ( image.Description.samplerCount > 1 );
        const bool isCubemap = ( image.Description.miscFlags & ImageDesc::IS_CUBE_MAP );

        if ( isCubemap ) {
            DUSK_LOG_ERROR( "Failed to create RTV: IsCubemap flag is set..." );
            return nullptr;
        }

        if ( isMultisampled ) {
            // TEXTURE2DMS
            if ( isArrayView ) {
                renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;

                renderTargetViewDesc.Texture2DMSArray.ArraySize = imgCount;
                renderTargetViewDesc.Texture2DMSArray.FirstArraySlice = viewDesc.StartImageIndex;
            } else {
                renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
            }
        } else {
            // TEXTURE2D
            if ( isArrayView ) {
                renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;

                renderTargetViewDesc.Texture2DArray.ArraySize = imgCount;
                renderTargetViewDesc.Texture2DArray.FirstArraySlice = viewDesc.StartImageIndex;

                renderTargetViewDesc.Texture2DArray.MipSlice = viewDesc.StartMipIndex;
            } else {
                renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

                renderTargetViewDesc.Texture2D.MipSlice = viewDesc.StartMipIndex;
            }
        }
    } break;
    case ImageDesc::DIMENSION_3D:
    {
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;

        renderTargetViewDesc.Texture3D.FirstWSlice = viewDesc.StartImageIndex;
        renderTargetViewDesc.Texture3D.WSize = viewDesc.ImageCount;

        renderTargetViewDesc.Texture3D.MipSlice = viewDesc.StartMipIndex;
    } break;
    }

    ID3D11RenderTargetView* renderTargetView = nullptr;
    HRESULT operationResult = device->CreateRenderTargetView( image.textureResource, &renderTargetViewDesc, &renderTargetView );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "RenderTargetView creation failed (error code: 0x%x)!", operationResult );

    return renderTargetView;
}

static ID3D11DepthStencilView* CreateImageDepthStencilView( ID3D11Device* device, const Image& image, const ImageViewDesc& viewDesc )
{
    const DXGI_FORMAT viewFormat = static_cast< DXGI_FORMAT >( viewDesc.ViewFormat );

    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    depthStencilViewDesc.Format = viewFormat;
    depthStencilViewDesc.Flags = 0;

    switch ( viewFormat ) {
    case DXGI_FORMAT_R16_TYPELESS:
        depthStencilViewDesc.Format = DXGI_FORMAT_D16_UNORM;
        break;
    case DXGI_FORMAT_R32_TYPELESS:
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        break;
    }

    const bool isArrayView = ( viewDesc.ImageCount > 1 );
    const UINT mipCount = ( viewDesc.MipCount <= 0 ) ? -1 : viewDesc.MipCount;
    const UINT imgCount = ( viewDesc.ImageCount <= 0 ) ? -1 : viewDesc.ImageCount;

    switch ( image.Description.dimension ) {
    case ImageDesc::DIMENSION_1D:
    {
        if ( isArrayView ) {
            depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;

            depthStencilViewDesc.Texture1DArray.ArraySize = imgCount;
            depthStencilViewDesc.Texture1DArray.FirstArraySlice = viewDesc.StartImageIndex;

            depthStencilViewDesc.Texture1DArray.MipSlice = viewDesc.StartMipIndex;
        } else {
            depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;

            depthStencilViewDesc.Texture1D.MipSlice = viewDesc.StartMipIndex;
        }
    } break;
    case ImageDesc::DIMENSION_2D:
    {
        const bool isMultisampled = ( image.Description.samplerCount > 1 );
        const bool isCubemap = ( image.Description.miscFlags & ImageDesc::IS_CUBE_MAP );

        if ( isCubemap ) {
            DUSK_LOG_ERROR( "Failed to create RTV: IsCubemap flag is set..." );
            return nullptr;
        }

        if ( isMultisampled ) {
            // TEXTURE2DMS
            if ( isArrayView ) {
                depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;

                depthStencilViewDesc.Texture2DMSArray.ArraySize = imgCount;
                depthStencilViewDesc.Texture2DMSArray.FirstArraySlice = viewDesc.StartImageIndex;
            } else {
                depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
            }
        } else {
            // TEXTURE2D
            if ( isArrayView ) {
                depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;

                depthStencilViewDesc.Texture2DArray.ArraySize = imgCount;
                depthStencilViewDesc.Texture2DArray.FirstArraySlice = viewDesc.StartImageIndex;

                depthStencilViewDesc.Texture2DArray.MipSlice = viewDesc.StartMipIndex;
            } else {
                depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

                depthStencilViewDesc.Texture2D.MipSlice = viewDesc.StartMipIndex;
            }
        }
    } break;
    }

    ID3D11DepthStencilView* depthStencilView = nullptr;
    HRESULT operationResult = device->CreateDepthStencilView( image.textureResource, &depthStencilViewDesc, &depthStencilView );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "DepthStencilView creation failed (error code: 0x%x)!", operationResult );

    return depthStencilView;
}

static ID3D11UnorderedAccessView* CreateBufferUnorderedAccessView( ID3D11Device* device, const Buffer& buffer, const BufferViewDesc& viewDesc )
{
    bool isFormatless = ( buffer.BindFlags & RESOURCE_BIND_STRUCTURED_BUFFER );

    D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc;
    unorderedAccessViewDesc.Format = isFormatless ? DXGI_FORMAT_UNKNOWN : static_cast<DXGI_FORMAT>( viewDesc.ViewFormat );
    unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    unorderedAccessViewDesc.Buffer.FirstElement = viewDesc.FirstElement;
    unorderedAccessViewDesc.Buffer.NumElements = viewDesc.NumElements;
    unorderedAccessViewDesc.Buffer.Flags = 0;
    
    if ( buffer.BindFlags & RESOURCE_BIND_APPEND_STRUCTURED_BUFFER ) {
        unorderedAccessViewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_APPEND;
    }

	if ( buffer.BindFlags & RESOURCE_BIND_RAW ) {
		unorderedAccessViewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
	}

    ID3D11UnorderedAccessView* unorderedAccessView = nullptr;
    HRESULT operationResult = device->CreateUnorderedAccessView( buffer.BufferObject, &unorderedAccessViewDesc, &unorderedAccessView );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "UnorderedAccessView creation failed (error code: 0x%x)!", operationResult );

    return unorderedAccessView;
}

static ID3D11ShaderResourceView* CreateBufferShaderResourceView( ID3D11Device* device, const Buffer& buffer, const BufferViewDesc& viewDesc )
{
    bool isFormatless = ( buffer.BindFlags & RESOURCE_BIND_STRUCTURED_BUFFER );

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = isFormatless ? DXGI_FORMAT_UNKNOWN : static_cast< DXGI_FORMAT >( viewDesc.ViewFormat );
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    shaderResourceViewDesc.Buffer.FirstElement = viewDesc.FirstElement;
    shaderResourceViewDesc.Buffer.NumElements = viewDesc.NumElements;

    ID3D11ShaderResourceView* shaderResourceView = nullptr;
    HRESULT operationResult = device->CreateShaderResourceView( buffer.BufferObject, &shaderResourceViewDesc, &shaderResourceView );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "ShaderResourceView creation failed (error code: 0x%x)!", operationResult );

    return shaderResourceView;
}
#endif
