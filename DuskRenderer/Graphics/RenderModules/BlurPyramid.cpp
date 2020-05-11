/*
Dusk Source Code
Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "BlurPyramid.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

static PipelineState*  g_DownsamplePipelineStateObject = nullptr;
static PipelineState*  g_KarisAveragePipelineStateObject = nullptr;
static PipelineState*  g_UpsamplePipelineStateObject = nullptr;

// Round dimension to a lower even number (avoid padding on GPU and should speed-up (down/up)sampling)
template<typename T>
DUSK_INLINE T RoundToEven( const T value )
{
    static_assert( std::is_integral<T>(), "T should be integral (or implement modulo operator)" );
    return ( ( value % 2 == static_cast<T>( 0 ) ) ? value : ( value - static_cast<T>( 1 ) ) );
}

void LoadCachedResourcesBP( RenderDevice& renderDevice, ShaderCache& shaderCache )
{
    PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
    psoDesc.computeShader = shaderCache.getOrUploadStageDynamic<SHADER_STAGE_COMPUTE>( "PostFX/Downsample" );
    psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

    g_DownsamplePipelineStateObject = renderDevice.createPipelineState( psoDesc );

    // Mip0 to mip1 pso
    psoDesc.computeShader = shaderCache.getOrUploadStageDynamic<SHADER_STAGE_COMPUTE>( "PostFX/Downsample+DUSK_USE_KARIS_AVERAGE" );

    g_KarisAveragePipelineStateObject = renderDevice.createPipelineState( psoDesc );

    // Upsample pso
    PipelineStateDesc upsamplePsoDesc( PipelineStateDesc::COMPUTE );
    upsamplePsoDesc.computeShader = shaderCache.getOrUploadStageDynamic<SHADER_STAGE_COMPUTE>( "PostFX/Upsample" );
    upsamplePsoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

    g_UpsamplePipelineStateObject = renderDevice.createPipelineState( upsamplePsoDesc );
}

void FreeCachedResourcesBP( RenderDevice& renderDevice )
{
    renderDevice.destroyPipelineState( g_DownsamplePipelineStateObject );
    renderDevice.destroyPipelineState( g_KarisAveragePipelineStateObject );
    renderDevice.destroyPipelineState( g_UpsamplePipelineStateObject );
}

static ResHandle_t* AddDownsampleMipchainRenderPass( FrameGraph& frameGraph, ResHandle_t input, const u32 inputWidth, const u32 inputHeight, const u32 mipCount = 1u )
{
    struct PassData {
        ResHandle_t input;
        ResHandle_t output[8];
    };

    DUSK_DEV_ASSERT( mipCount <= 8, "MipChain length limit reached! Increase the max mip count if needed" )

    PassData& passData = frameGraph.addRenderPass<PassData>(
        "Downsampled Mipchain Pass",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.input = builder.readImage( input );
            
            ImageDesc texDesc;
            texDesc.dimension = ImageDesc::DIMENSION_2D;
            texDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            texDesc.depth = 1;
            texDesc.mipCount = 1;
            texDesc.arraySize = 1;
            texDesc.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            texDesc.usage = RESOURCE_USAGE_DEFAULT;
            texDesc.samplerCount = 1;

            u32 downsampleFactor = 2u;
            for ( u32 i = 0; i < mipCount; i++ ) {
                const f32 downsample = ( 1.0f / downsampleFactor );

                texDesc.width = RoundToEven( static_cast< u32 >( inputWidth * downsample ) );
                texDesc.height = RoundToEven( static_cast< u32 >( inputHeight * downsample ) );

                passData.output[i] = builder.allocateImage( texDesc );
                
                downsampleFactor <<= 1;
            }
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            // Mip0 need to use a dedicated pso permutation (for filtering stabilization)
            Image* renderTargetMip0 = resources->getImage( passData.output[0] );
            Image* inputTarget = resources->getImage( passData.input );

            cmdList->pushEventMarker( DUSK_STRING( "Weighted Downsample (Mip0) Pass" ) );
            cmdList->bindPipelineState( g_KarisAveragePipelineStateObject );

            cmdList->bindImage( DUSK_STRING_HASH( "g_TextureInput" ), inputTarget );
            cmdList->bindImage( DUSK_STRING_HASH( "g_OutputRenderTarget" ), renderTargetMip0 );

            cmdList->prepareAndBindResourceList( g_KarisAveragePipelineStateObject );

            cmdList->dispatchCompute( 
                static_cast< u32 >( inputWidth ) / 16u, 
                static_cast< u32 >( inputHeight ) / 16u, 
                1u
            );
            cmdList->insertComputeBarrier( *renderTargetMip0 );

            cmdList->popEventMarker();

            // Compute the rest of the mipchain
            cmdList->pushEventMarker( DUSK_STRING( "Weighted Downsample Pass" ) );
            cmdList->bindPipelineState( g_DownsamplePipelineStateObject );

            u32 width = inputWidth >> 1;
            u32 height = inputHeight >> 1;

            // TODO Merge each mip compute into a single dispatch call
            for ( u32 i = 1u; i < mipCount; i++ ) {
                Image* renderTarget = resources->getImage( passData.output[i] );

                cmdList->bindImage( DUSK_STRING_HASH( "g_TextureInput" ), resources->getImage( passData.output[i - 1] ) );
                cmdList->bindImage( DUSK_STRING_HASH( "g_OutputRenderTarget" ), renderTarget );

                cmdList->prepareAndBindResourceList( g_DownsamplePipelineStateObject );

                cmdList->dispatchCompute(
                    static_cast< u32 >( width ) / 16u,
                    static_cast< u32 >( height ) / 16u,
                    1u
                );
                cmdList->insertComputeBarrier( *renderTarget );

                width >>= 1;
                height >>= 1;
            }

            cmdList->popEventMarker();
        }
    );

    return passData.output;
}

static ResHandle_t AddUpsampleMipRenderPass( FrameGraph& frameGraph, ResHandle_t* inputMipChain, const u32 inputWidth, const u32 inputHeight, const u32 mipCount = 1u )
{
    struct PassData {
        ResHandle_t input[8];
        ResHandle_t output[8];

        ResHandle_t upsampleInfosBuffer;
    };

    struct UpsampleInfos {
        dkVec2f       inverseTextureDimensions; // = 1.0f / Input texture 
        f32           filterRadius; // NOTE The lower the resolution is, the bigger the radius should be
    };

    DUSK_DEV_ASSERT( mipCount <= 8, "MipChain length limit reached! Increase the max mip count if needed" )

    PassData& passData = frameGraph.addRenderPass<PassData>(
        "Upsample Weighted Pass",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            for ( u32 i = 0; i < mipCount; i++ ) {
                passData.input[i] = builder.readImage( inputMipChain[i] );
            }

            ImageDesc texDesc;
            texDesc.dimension = ImageDesc::DIMENSION_2D;
            texDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            texDesc.depth = 1;
            texDesc.mipCount = 1;
            texDesc.arraySize = 1;
            texDesc.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            texDesc.usage = RESOURCE_USAGE_DEFAULT;
            texDesc.samplerCount = 1;

            u32 downsampleFactor = ( mipCount - 1 ) << 2;
            for ( u32 i = 0; i < mipCount; i++ ) {
                const f32 downsample = ( 1.0f / downsampleFactor );

                texDesc.width = RoundToEven( static_cast< u32 >( inputWidth * downsample ) );
                texDesc.height = RoundToEven( static_cast< u32 >( inputHeight * downsample ) );

                passData.output[i] = builder.allocateImage( texDesc );

                downsampleFactor >>= 1;
            }

            BufferDesc bufferDesc;
            bufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            bufferDesc.SizeInBytes = sizeof( dkVec4f );
            bufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;

            passData.upsampleInfosBuffer = builder.allocateBuffer( bufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* upsampleBuffer = resources->getBuffer( passData.upsampleInfosBuffer );

            // Compute the rest of the mipchain
            cmdList->pushEventMarker( DUSK_STRING( "Weighted Upsample Pass" ) );
            cmdList->bindPipelineState( g_UpsamplePipelineStateObject );

            cmdList->bindConstantBuffer( DUSK_STRING_HASH( "g_UpsampleInfos" ), upsampleBuffer );

            // TODO Merge each mip compute into a single dispatch call
            f32 filterRadius = 2.0f;

            u32 initialMipIdx = ( mipCount - 1 );
            u32 width = inputWidth >> initialMipIdx;
            u32 height = inputHeight >> initialMipIdx;

            for ( u32 i = 0u; i < mipCount; i++ ) {
                UpsampleInfos upsampleInfos;
                upsampleInfos.filterRadius = filterRadius;
                upsampleInfos.inverseTextureDimensions.x = 1.0f / width;
                upsampleInfos.inverseTextureDimensions.y = 1.0f / height;
                cmdList->updateBuffer( *upsampleBuffer, &upsampleInfos, sizeof( UpsampleInfos ) );

                cmdList->bindImage( DUSK_STRING_HASH( "g_TextureInput" ), resources->getImage( passData.input[mipCount - ( 1 + i )] ) );
                cmdList->bindImage( DUSK_STRING_HASH( "g_OutputRenderTarget" ), resources->getImage( passData.output[i] ) );

                cmdList->prepareAndBindResourceList( g_UpsamplePipelineStateObject );

                cmdList->dispatchCompute(
                    width / 16u,
                    height / 16u,
                    1u
                );

                width <<= 1;
                height <<= 1;

                filterRadius -= 0.25f;
            }

            cmdList->popEventMarker();
        }
    );

    return passData.output[mipCount - 1];
}

ResHandle_t AddBlurPyramidRenderPass( FrameGraph& frameGraph, ResHandle_t input, u32 inputWidth, u32 inputHeight )
{
    // Downsample the mipchain
    ResHandle_t* downsampleMipchain = AddDownsampleMipchainRenderPass( frameGraph, input, inputWidth, inputHeight, 5u );
    
    // Upsample and accumulate mips (using hardware blending)
    ResHandle_t upsampleTarget = AddUpsampleMipRenderPass( frameGraph, downsampleMipchain, inputWidth, inputHeight, 5u );

    return upsampleTarget;
}
