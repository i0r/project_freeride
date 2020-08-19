/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Core/ViewFormat.h>

#include "RenderDevice.h"
#include "CommandList.h"

#include "PipelineState.h"
#include "Image.h"
#include "ResourceAllocationHelpers.h"

#include <Maths/Helpers.h>
#include <Core/StringHelpers.h>

#include <d3d11.h>
#include <vector>

bool IsUsingCompressedFormat( const eViewFormat format )
{
    return format == VIEW_FORMAT_BC1_TYPELESS
        || format == VIEW_FORMAT_BC1_UNORM
        || format == VIEW_FORMAT_BC1_UNORM_SRGB
        || format == VIEW_FORMAT_BC2_TYPELESS
        || format == VIEW_FORMAT_BC2_UNORM
        || format == VIEW_FORMAT_BC2_UNORM_SRGB
        || format == VIEW_FORMAT_BC3_TYPELESS
        || format == VIEW_FORMAT_BC3_UNORM
        || format == VIEW_FORMAT_BC3_UNORM_SRGB
        || format == VIEW_FORMAT_BC4_TYPELESS
        || format == VIEW_FORMAT_BC4_UNORM
        || format == VIEW_FORMAT_BC4_SNORM
        || format == VIEW_FORMAT_BC5_TYPELESS
        || format == VIEW_FORMAT_BC5_UNORM
        || format == VIEW_FORMAT_BC5_SNORM;
}

size_t BitsPerPixel( _In_ DXGI_FORMAT fmt )
{
    switch ( fmt ) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
        return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
        return 8;

    case DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 8;

    default:
        return 0;
    }
}

void GetSurfaceInfo( size_t width, size_t height, DXGI_FORMAT fmt, size_t* outNumBytes, size_t* outRowBytes, size_t* outNumRows )
{
    size_t numBytes = 0;
    size_t rowBytes = 0;
    size_t numRows = 0;

    bool bc = false;
    bool packed = false;
    bool planar = false;
    size_t bpe = 0;
    switch ( fmt ) {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        bc = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        bc = true;
        bpe = 16;
        break;

    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_YUY2:
        packed = true;
        bpe = 4;
        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        packed = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
        planar = true;
        bpe = 2;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        planar = true;
        bpe = 4;
        break;
    }

    if ( bc ) {
        size_t numBlocksWide = 0;
        if ( width > 0 ) {
            numBlocksWide = Max( 1, ( width + 3 ) / 4 );
        }
        size_t numBlocksHigh = 0;
        if ( height > 0 ) {
            numBlocksHigh = Max( 1, ( height + 3 ) / 4 );
        }
        rowBytes = numBlocksWide * bpe;
        numRows = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    } else if ( packed ) {
        rowBytes = ( ( width + 1 ) >> 1 ) * bpe;
        numRows = height;
        numBytes = rowBytes * height;
    } else if ( fmt == DXGI_FORMAT_NV11 ) {
        rowBytes = ( ( width + 3 ) >> 2 ) * 4;
        numRows = height * 2; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
        numBytes = rowBytes * numRows;
    } else if ( planar ) {
        rowBytes = ( ( width + 1 ) >> 1 ) * bpe;
        numBytes = ( rowBytes * height ) + ( ( rowBytes * height + 1 ) >> 1 );
        numRows = height + ( ( height + 1 ) >> 1 );
    } else {
        size_t bpp = BitsPerPixel( fmt );
        rowBytes = ( width * bpp + 7 ) / 8; // round up to nearest byte
        numRows = height;
        numBytes = rowBytes * height;
    }

    if ( outNumBytes ) {
        *outNumBytes = numBytes;
    }
    if ( outRowBytes ) {
        *outRowBytes = rowBytes;
    }
    if ( outNumRows ) {
        *outNumRows = numRows;
    }
}

void GetSubResourceDescriptor( std::vector<D3D11_SUBRESOURCE_DATA>& subResourceData, const void* initialData, const ImageDesc& description, const DXGI_FORMAT nativeTextureFormat )
{
    size_t NumBytes = 0;
    size_t RowBytes = 0;
    size_t index = 0;

    uint8_t* srcBits = ( uint8_t* )initialData;
    for ( u32 i = 0; i < description.arraySize; i++ ) {
        size_t w = description.width;
        size_t h = description.height;
        size_t d = description.depth;
        for ( u32 j = 0; j < description.mipCount; j++ ) {
            GetSurfaceInfo( w,
                h,
                nativeTextureFormat,
                &NumBytes,
                &RowBytes,
                nullptr
            );

            subResourceData[index].pSysMem = ( const void* )srcBits;
            subResourceData[index].SysMemPitch = static_cast< UINT >( RowBytes );
            subResourceData[index].SysMemSlicePitch = static_cast< UINT >( NumBytes );
            ++index;

            srcBits += NumBytes * d;

            w = w >> 1;
            h = h >> 1;
            d = d >> 1;

            if ( w == 0 ) {
                w = 1;
            }
            if ( h == 0 ) {
                h = 1;
            }
            if ( d == 0 ) {
                d = 1;
            }
        }
    }
}

Image* RenderDevice::createImage( const ImageDesc& description, const void* initialData, const size_t initialDataSize )
{
    ID3D11Device* device = renderContext->PhysicalDevice;

    UINT bindFlags = GetNativeBindFlags( description.bindFlags );
    UINT miscFlags = GetMiscFlags( description.bindFlags, description.miscFlags );
    UINT cpuFlags = GetCPUUsageFlags( description.usage );
    D3D11_USAGE usage = GetNativeUsage( description.usage );

    UINT sampleDescQuality = ( description.samplerCount > 1 ) ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    DXGI_FORMAT nativeTextureFormat = static_cast< DXGI_FORMAT >( description.format );

    std::vector<D3D11_SUBRESOURCE_DATA> subResourceData( Max( 1u, description.mipCount ) * description.arraySize );
    GetSubResourceDescriptor( subResourceData, initialData, description, nativeTextureFormat );

    Image* image = dk::core::allocate<Image>( memoryAllocator );
    image->Description = description;

    HRESULT textureCreationResult = S_OK;
    if ( description.dimension == ImageDesc::DIMENSION_1D ) {
        const D3D11_TEXTURE1D_DESC renderTargetDesc = {
            description.width,				// UINT Width
            description.mipCount,			// UINT MipLevels
            description.arraySize,          // UINT ArraySize
            nativeTextureFormat,	        // DXGI_FORMAT Format
            usage,			                // D3D11_USAGE Usage
            bindFlags,		                // UINT BindFlags
            cpuFlags,			            // UINT CPUAccessFlags
            miscFlags,  					// UINT MiscFlags
        };

        textureCreationResult = device->CreateTexture1D( &renderTargetDesc, ( initialData != nullptr ) ? subResourceData.data() : nullptr, &image->texture1D );
    } else if ( description.dimension == ImageDesc::DIMENSION_2D ) {
        const D3D11_TEXTURE2D_DESC renderTargetDesc = {
            description.width,				// UINT Width
            description.height,				// UINT Height
            description.mipCount,			// UINT MipLevels
            description.arraySize,			// UINT ArraySize
            nativeTextureFormat,	        // DXGI_FORMAT Format
            {								// DXGI_SAMPLE_DESC SampleDesc
                description.samplerCount,	//		UINT Count
                sampleDescQuality, 	        //		UINT Quality
            },
            usage,			                // D3D11_USAGE Usage
            bindFlags,		                // UINT BindFlags
            cpuFlags,			            // UINT CPUAccessFlags
            miscFlags,  					// UINT MiscFlags
        };

        textureCreationResult = device->CreateTexture2D( &renderTargetDesc, ( initialData != nullptr ) ? subResourceData.data() : nullptr, &image->texture2D );
    } else if ( description.dimension == ImageDesc::DIMENSION_3D ) {
        const D3D11_TEXTURE3D_DESC renderTargetDesc = {
            description.width,				// UINT Width
            description.height,				// UINT Height
            description.depth,			    // UINT ArraySize
            description.mipCount,			// UINT MipLevels
            nativeTextureFormat,	        // DXGI_FORMAT Format
            usage,			                // D3D11_USAGE Usage
            bindFlags,		                // UINT BindFlags
            cpuFlags,			            // UINT CPUAccessFlags
            miscFlags,  					// UINT MiscFlags
        };

        textureCreationResult = device->CreateTexture3D( &renderTargetDesc, ( initialData != nullptr ) ? subResourceData.data() : nullptr, &image->texture3D );
    }

    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( textureCreationResult ), "Texture creation failed! (error code 0x%X)", textureCreationResult );

    // We need to copy the default view descriptor since the input descriptor is immutable (we could use const_cast crap tho)
    ImageViewDesc defaultViewDesc = description.DefaultView;
        
    // Assign default view ViewFormat (if there is none)
    const eViewFormat defaultViewFormat = defaultViewDesc.ViewFormat;
    defaultViewDesc.ViewFormat = ( defaultViewFormat == VIEW_FORMAT_UNKNOWN ) ? description.format : defaultViewFormat;

    // Create default SRV
    if ( description.bindFlags & RESOURCE_BIND_SHADER_RESOURCE ) {
        image->DefaultShaderResourceView = CreateImageShaderResourceView( renderContext->PhysicalDevice, *image, defaultViewDesc );

        image->SRVs[defaultViewDesc.SortKey] = image->DefaultShaderResourceView;
    }

    if ( !( description.miscFlags & ImageDesc::IS_CUBE_MAP ) ) {
        // Create default UAV
        if ( description.bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
            image->DefaultUnorderedAccessView = CreateImageUnorderedAccessView( renderContext->PhysicalDevice, *image, defaultViewDesc );

            image->UAVs[defaultViewDesc.SortKey] = image->DefaultUnorderedAccessView;
        }

        // Create default RTV
        if ( description.bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ) {
            image->DefaultRenderTargetView = CreateImageRenderTargetView( renderContext->PhysicalDevice, *image, defaultViewDesc );

            image->RTVs[defaultViewDesc.SortKey].RenderTargetView = image->DefaultRenderTargetView;
        }

        // Create default DSV
        if ( description.bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) {
            image->DefaultDepthStencilView = CreateImageDepthStencilView( renderContext->PhysicalDevice, *image, defaultViewDesc );

            image->RTVs[defaultViewDesc.SortKey].DepthStencilView = image->DefaultDepthStencilView;
        }
    }

    return image;
}

void RenderDevice::createImageView( Image& image, const ImageViewDesc& viewDescription, const u32 creationFlags )
{
	if ( creationFlags & IMAGE_VIEW_CREATE_UAV ) {
		if ( creationFlags & IMAGE_VIEW_COVER_WHOLE_MIPCHAIN ) {
			ImageViewDesc perMipViewDescription = viewDescription;
			for ( u32 i = 0; i < image.Description.mipCount; i++ ) {
				perMipViewDescription.StartMipIndex = i;
				perMipViewDescription.MipCount = 1;

				image.UAVs[perMipViewDescription.SortKey] = CreateImageUnorderedAccessView( renderContext->PhysicalDevice, image, perMipViewDescription );
			}
		} else {
			image.UAVs[viewDescription.SortKey] = CreateImageUnorderedAccessView( renderContext->PhysicalDevice, image, viewDescription );
        }
	}

	if ( creationFlags & IMAGE_VIEW_CREATE_SRV ) {
		if ( creationFlags & IMAGE_VIEW_COVER_WHOLE_MIPCHAIN ) {
			ImageViewDesc perMipViewDescription = viewDescription;
			for ( u32 i = 0; i < image.Description.mipCount; i++ ) {
				perMipViewDescription.StartMipIndex = i;
				perMipViewDescription.MipCount = 1;

				image.SRVs[perMipViewDescription.SortKey] = CreateImageShaderResourceView( renderContext->PhysicalDevice, image, perMipViewDescription );
			}
		} else {
			image.SRVs[viewDescription.SortKey] = CreateImageShaderResourceView( renderContext->PhysicalDevice, image, viewDescription );
		}
	}

	if ( creationFlags & IMAGE_VIEW_CREATE_RTV_OR_DSV ) {
        if ( image.Description.bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ) {
            if ( creationFlags & IMAGE_VIEW_COVER_WHOLE_MIPCHAIN ) {
                ImageViewDesc perMipViewDescription = viewDescription;
                for ( u32 i = 0; i < image.Description.mipCount; i++ ) {
                    perMipViewDescription.StartMipIndex = i;
                    perMipViewDescription.MipCount = 1;

					image.RTVs[perMipViewDescription.SortKey].RenderTargetView = CreateImageRenderTargetView( renderContext->PhysicalDevice, image, perMipViewDescription );
                }
            } else {
				image.RTVs[viewDescription.SortKey].RenderTargetView = CreateImageRenderTargetView( renderContext->PhysicalDevice, image, viewDescription );
            }
		} else if ( image.Description.bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) {
			if ( creationFlags & IMAGE_VIEW_COVER_WHOLE_MIPCHAIN ) {
				ImageViewDesc perMipViewDescription = viewDescription;
				for ( u32 i = 0; i < image.Description.mipCount; i++ ) {
					perMipViewDescription.StartMipIndex = i;
					perMipViewDescription.MipCount = 1;

					image.RTVs[perMipViewDescription.SortKey].DepthStencilView = CreateImageDepthStencilView( renderContext->PhysicalDevice, image, perMipViewDescription );
				}
			} else {
				image.RTVs[viewDescription.SortKey].DepthStencilView = CreateImageDepthStencilView( renderContext->PhysicalDevice, image, viewDescription );
			}
        }
    }
}

void CommandList::copyImage( Image& src, Image& dst, const ImageViewDesc& infosSrc, const ImageViewDesc& infosDst )
{
    CommandPacket::CopyImage* commandPacket = dk::core::allocate<CommandPacket::CopyImage>( nativeCommandList->CommandPacketAllocator );
    memset( commandPacket, 0, sizeof( CommandPacket::CopyImage ) );

    commandPacket->Identifier = CPI_COPY_IMAGE;
    commandPacket->SourceImage = src.textureResource;
    commandPacket->DestImage = dst.textureResource;

    const UINT sourceResource = D3D11CalcSubresource( infosSrc.StartMipIndex, infosSrc.StartImageIndex, infosSrc.MipCount );
    const UINT destResource = D3D11CalcSubresource( infosDst.StartMipIndex, infosDst.StartImageIndex, infosDst.MipCount );
    commandPacket->SourceResourceIndex = sourceResource;
    commandPacket->DestResourceIndex = destResource;

    nativeCommandList->Commands.push( reinterpret_cast< u32* >( commandPacket ) );
}

void RenderDevice::destroyImage( Image* image )
{
#define RELEASE_IF_ALLOCATED( obj ) if ( obj != nullptr ) { obj->Release(); obj = nullptr; }

	for ( auto& srv : image->SRVs ) {
		RELEASE_IF_ALLOCATED( srv.second );
	}
    image->SRVs.clear();

	for ( auto& uav : image->UAVs ) {
		RELEASE_IF_ALLOCATED( uav.second );
	}
	image->UAVs.clear();

	for ( auto& rtv : image->RTVs ) {
		RELEASE_IF_ALLOCATED( rtv.second.RenderTargetView );
	}
	image->RTVs.clear();

    // Those are shortcuts to the actual hashmap entry.
    // We simply need to remove the reference for gc.
	image->DefaultShaderResourceView = nullptr;
	image->DefaultUnorderedAccessView = nullptr;
    image->DefaultRenderTargetView = nullptr;
	
    switch ( image->Description.dimension ) {
    case ImageDesc::DIMENSION_1D:
        RELEASE_IF_ALLOCATED( image->texture1D );
        break;
    case ImageDesc::DIMENSION_2D:
        RELEASE_IF_ALLOCATED( image->texture2D );
        break;
    case ImageDesc::DIMENSION_3D:
        RELEASE_IF_ALLOCATED( image->texture3D );
        break;
    };

    dk::core::free( memoryAllocator, image );
#undef RELEASE_IF_ALLOCATED
}

void RenderDevice::setDebugMarker( Image& image, const dkChar_t* objectName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    image.textureResource->SetPrivateData( WKPDID_D3DDebugObjectName, static_cast< UINT >( wcslen( objectName ) ), WideStringToString( objectName ).c_str() );
#endif
}

void CommandList::transitionImage( Image& image, const eResourceState state, const u32 mipIndex, const TransitionType transitionType )
{
    // Resource state is managed at driver level in D3D11
}

void CommandList::insertComputeBarrier( Image& image )
{

}

void CommandList::resolveImage( Image& src, Image& dst )
{
    CommandPacket::ResolveImage* commandPacket = dk::core::allocate<CommandPacket::ResolveImage>( nativeCommandList->CommandPacketAllocator );
    memset( commandPacket, 0, sizeof( CommandPacket::ResolveImage ) );

    DXGI_FORMAT imageFormat = static_cast< DXGI_FORMAT >( src.Description.format );
    commandPacket->Identifier = CPI_RESOLVE_IMAGE;
    commandPacket->Format = imageFormat;
    commandPacket->SourceImage = src.textureResource;
    commandPacket->DestImage = dst.textureResource;

    nativeCommandList->Commands.push( reinterpret_cast< u32* >( commandPacket ) );
}

void CommandList::setupFramebuffer( FramebufferAttachment* renderTargetViews, FramebufferAttachment depthStencilView )
{
    CommandPacket::SetupFramebuffer* commandPacket = dk::core::allocate<CommandPacket::SetupFramebuffer>( nativeCommandList->CommandPacketAllocator );
    memset( commandPacket, 0, sizeof( CommandPacket::SetupFramebuffer ) );

    commandPacket->Identifier = CPI_SETUP_FRAMEBUFFER;

    memcpy( commandPacket->RenderTargetView, renderTargetViews, sizeof( FramebufferAttachment ) * nativeCommandList->BindedPipelineState->rtvCount );
    commandPacket->DepthStencilView = depthStencilView;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::clearRenderTargets( Image** renderTargetViews, const u32 renderTargetCount, const f32 clearValues[4] )
{
    CommandPacket::ClearImage* commandPacket = dk::core::allocate<CommandPacket::ClearImage>( nativeCommandList->CommandPacketAllocator );
    memset( commandPacket, 0, sizeof( CommandPacket::ClearImage ) );

    commandPacket->Identifier = CPI_CLEAR_IMAGES;
    commandPacket->ImageCount = renderTargetCount;

    memcpy( commandPacket->RenderTargetView, renderTargetViews, sizeof( Image* ) * renderTargetCount );
    memcpy( commandPacket->ClearValue, clearValues, sizeof( f32 ) * 4 );

    nativeCommandList->Commands.push( reinterpret_cast< u32* >( commandPacket ) );
}

void CommandList::clearDepthStencil( Image* depthStencilView, const f32 clearValue, const bool clearStencil, const u8 clearStencilValue )
{
    CommandPacket::ClearDepthStencil* commandPacket = dk::core::allocate<CommandPacket::ClearDepthStencil>( nativeCommandList->CommandPacketAllocator );
    memset( commandPacket, 0, sizeof( CommandPacket::ClearDepthStencil ) );

    commandPacket->Identifier = CPI_CLEAR_DEPTH_STENCIL;
    commandPacket->DepthStencil = depthStencilView;
    commandPacket->ClearValue = clearValue;
    commandPacket->ClearStencil = clearStencil;
    commandPacket->ClearStencilValue = clearStencilValue;

    nativeCommandList->Commands.push( reinterpret_cast< u32* >( commandPacket ) );
}

void SetupFramebuffer_Replay( RenderContext* renderContext, FramebufferAttachment* renderTargetViews, FramebufferAttachment depthStencilView )
{
    ID3D11RenderTargetView* RenderTargets[8] = { nullptr };
    ID3D11DepthStencilView* DepthStencilView = nullptr;

    if ( renderContext->FramebufferDepthBuffer != nullptr ) {
        renderContext->FramebufferDepthBuffer->IsBindedToFBO = false;
        renderContext->FramebufferDepthBuffer = nullptr;
    }

    Image* dsvImage = depthStencilView.ImageAttachment;
    if ( dsvImage != nullptr ) {
        const u64 viewKey = depthStencilView.ViewDescription.SortKey;
        DepthStencilView = ( viewKey != 0ull ) ? dsvImage->RTVs[viewKey].DepthStencilView : dsvImage->DefaultDepthStencilView;

        dsvImage->IsBindedToFBO = true;
        renderContext->FramebufferDepthBuffer = dsvImage;
    }

    const PipelineState* bindedPipelineState = renderContext->BindedPipelineState;
    const i32 rtvCount = bindedPipelineState->rtvCount;

    ID3D11DeviceContext* immediateCtx = renderContext->ImmediateContext;

    bool needUavFlush = false;
    bool needSrvFlush = false;

    i32 attachmentIdx = 0;
    for ( ; attachmentIdx < rtvCount; attachmentIdx++ ) {
        Image* rtvImage = renderTargetViews[attachmentIdx].ImageAttachment;

        if ( ClearImageUAVRegister( renderContext, rtvImage ) ) {
            needUavFlush = true;
        }

        if ( ClearImageSRVRegister( renderContext, rtvImage ) ) {
            needSrvFlush = true;
        }

        if ( renderContext->FramebufferAttachment[attachmentIdx] != nullptr 
          && renderContext->FramebufferAttachment[attachmentIdx]->IsBindedToFBO ) {
            renderContext->FramebufferAttachment[attachmentIdx]->IsBindedToFBO = false;
        }

        renderContext->FramebufferAttachment[attachmentIdx] = rtvImage;
        renderContext->FramebufferAttachment[attachmentIdx]->IsBindedToFBO = true;

        const u64 viewKey = renderTargetViews[attachmentIdx].ViewDescription.SortKey;
        RenderTargets[attachmentIdx] = ( viewKey != 0ull ) ? rtvImage->RTVs[viewKey].RenderTargetView : rtvImage->DefaultRenderTargetView;

        if ( bindedPipelineState->clearRtv[attachmentIdx] ) {
            immediateCtx->ClearRenderTargetView( RenderTargets[attachmentIdx], bindedPipelineState->colorClearValue );
        }
    }

    // Clear FBO attachments
    for ( ; attachmentIdx < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; attachmentIdx++ ) {
        if ( renderContext->FramebufferAttachment[attachmentIdx] != nullptr
          && renderContext->FramebufferAttachment[attachmentIdx]->IsBindedToFBO ) {
            renderContext->FramebufferAttachment[attachmentIdx]->IsBindedToFBO = false;
            renderContext->FramebufferAttachment[attachmentIdx] = nullptr;
        }

        RenderTargets[attachmentIdx] = nullptr;
    }

    if ( needUavFlush ) {
        FlushUAVRegisterUpdate( renderContext );
    }

    if ( needSrvFlush ) {
        FlushSRVRegisterUpdate( renderContext );
    }

    if ( bindedPipelineState->clearDsv ) {
        immediateCtx->ClearDepthStencilView( DepthStencilView, D3D11_CLEAR_DEPTH, bindedPipelineState->depthClearValue, bindedPipelineState->stencilClearValue );
    }

    if ( renderContext->PsUavRegisterUpdateCount == 0 ) {
        // If we don't need to bind UAVs, use OMSetRenderTargets.
        immediateCtx->OMSetRenderTargets( rtvCount, RenderTargets, DepthStencilView );
    } else {
        // Shift registers to match UAVStartSlot (should be equal to the number of render-target views being bound).
        ID3D11UnorderedAccessView** ShiftedUAVRegisters = &renderContext->PsUavRegisters[rtvCount];
       
        immediateCtx->OMSetRenderTargetsAndUnorderedAccessViews( rtvCount, RenderTargets, DepthStencilView, rtvCount, renderContext->PsUavRegisterUpdateCount, ShiftedUAVRegisters, nullptr );
        
        renderContext->PsUavRegisterUpdateCount = 0;
        renderContext->PsUavRegisterUpdateStart = ~0;
    }
}
#endif
