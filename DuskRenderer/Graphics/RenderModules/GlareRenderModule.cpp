/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "GlareRenderModule.h"

#include <Graphics/FrameGraph.h>
#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>

#include "Generated/FFT.generated.h"

GlareRenderModule::GlareRenderModule()
    : glarePatternTexture( nullptr )
    , glarePatternFFT{ nullptr, nullptr }
{

}

GlareRenderModule::~GlareRenderModule()
{
    glarePatternTexture = nullptr;

    for ( i32 i = 0; i < 2; i++ ) {
        glarePatternFFT[i] = nullptr;
    }
}

FFTPassOutput GlareRenderModule::addGlareComputePass( FrameGraph& frameGraph, FFTPassOutput fftInput )
{
    struct PassData {
        ResHandle_t inputReal;
        ResHandle_t inputImaginary;
        ResHandle_t outputReal;
        ResHandle_t outputImaginary;
    };

    PassData rowPassData = frameGraph.addRenderPass<PassData>(
        FFT::FrequencyDomainMul_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            passData.inputReal = builder.readReadOnlyImage( fftInput.RealPart );
            passData.inputImaginary = builder.readReadOnlyImage( fftInput.ImaginaryPart );

            ImageDesc imageDesc;
            imageDesc.dimension = ImageDesc::DIMENSION_2D;
            imageDesc.format = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;
            imageDesc.width = FFT_TEXTURE_DIMENSION;
            imageDesc.height = FFT_TEXTURE_DIMENSION;
            imageDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;
            imageDesc.bindFlags = eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;

            passData.outputReal = builder.allocateImage( imageDesc );
            passData.outputImaginary = builder.allocateImage( imageDesc );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputReal = resources->getImage( passData.outputReal );
            Image* outputImaginary = resources->getImage( passData.outputImaginary );
            Image* inputReal = resources->getImage( passData.inputReal );
            Image* inputImaginary = resources->getImage( passData.inputImaginary );

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::FrequencyDomainMul_ShaderBinding );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( FFT::FrequencyDomainMul_EventName );

            // Bind resources
            cmdList->bindImage( FFT::FrequencyDomainMul_TextureTargetR_Hashcode, outputReal );
            cmdList->bindImage( FFT::FrequencyDomainMul_TextureTargetI_Hashcode, outputImaginary );

            cmdList->bindImage( FFT::FrequencyDomainMul_TextureSourceI_Hashcode, inputImaginary );
            cmdList->bindImage( FFT::FrequencyDomainMul_TextureSourceR_Hashcode, inputReal );
            cmdList->bindImage( FFT::FrequencyDomainMul_Texture2SourceI_Hashcode, glarePatternFFT[1] );
            cmdList->bindImage( FFT::FrequencyDomainMul_Texture2SourceR_Hashcode, glarePatternFFT[0] );

            cmdList->prepareAndBindResourceList( pso );

            cmdList->dispatchCompute( FFT_TEXTURE_DIMENSION / FFT::FrequencyDomainMul_DispatchX, FFT_TEXTURE_DIMENSION / FFT::FrequencyDomainMul_DispatchY, FFT::FrequencyDomainMul_DispatchZ);

            cmdList->popEventMarker();
        }
    );

    FFTPassOutput output;
    output.RealPart = rowPassData.outputReal;
    output.ImaginaryPart = rowPassData.outputImaginary;

    return output;
}

void GlareRenderModule::destroy( RenderDevice& renderDevice )
{
    for ( u32 i = 0; i < 2u; i++ ) {
        renderDevice.destroyImage( glarePatternFFT[i] );
    }
}

void GlareRenderModule::loadCachedResources( RenderDevice& renderDevice, ShaderCache& shaderCache, GraphicsAssetCache& graphicsAssetCache )
{
    glarePatternTexture = graphicsAssetCache.getImage( DUSK_STRING( "GameData/textures/default_glare_pattern.png" ) );

    ImageDesc imageDesc;
    imageDesc.dimension = ImageDesc::DIMENSION_2D;
    imageDesc.format = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;
    imageDesc.width = FFT_TEXTURE_DIMENSION;
    imageDesc.height = FFT_TEXTURE_DIMENSION;
    imageDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;
    imageDesc.bindFlags = eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;

    for ( i32 i = 0; i < 2; i++ ) {
        glarePatternFFT[i] = renderDevice.createImage( imageDesc );
    }
}

void GlareRenderModule::precomputePipelineResources( FrameGraph& frameGraph )
{
    struct PassData {
        ResHandle_t outputReal;
        ResHandle_t outputImaginary;
    };

    PassData rowPassData = frameGraph.addRenderPass<PassData>(
        FFT::FFTComputeRow_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            ImageDesc imageDesc;
            imageDesc.dimension = ImageDesc::DIMENSION_2D;
            imageDesc.format = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;
            imageDesc.width = FFT_TEXTURE_DIMENSION;
            imageDesc.height = FFT_TEXTURE_DIMENSION;
            imageDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;
            imageDesc.bindFlags = eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;

            passData.outputReal = builder.allocateImage( imageDesc );
            passData.outputImaginary = builder.allocateImage( imageDesc );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputReal = resources->getImage( passData.outputReal );
            Image* outputImaginary = resources->getImage( passData.outputImaginary );

            // Row FFT compute
            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::FFTComputeRow_ShaderBinding );
            cmdList->pushEventMarker( FFT::FFTComputeRow_EventName );
            cmdList->bindPipelineState( pso );

            // Bind resources
            cmdList->bindImage( FFT::FFTComputeRow_TextureSourceR_Hashcode, glarePatternTexture );

            cmdList->bindImage( FFT::FFTComputeRow_TextureTargetR_Hashcode, outputReal );
            cmdList->bindImage( FFT::FFTComputeRow_TextureTargetI_Hashcode, outputImaginary );

            cmdList->prepareAndBindResourceList( pso );

            cmdList->dispatchCompute( 1, FFT_TEXTURE_DIMENSION, 1 );

            cmdList->popEventMarker();

            // Column FFT compute.
            pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::FFTComputeCol_ShaderBinding );
            cmdList->pushEventMarker( FFT::FFTComputeCol_EventName );
            cmdList->bindPipelineState( pso );

            // Bind resources
            cmdList->bindImage( FFT::FFTComputeCol_TextureSourceR_Hashcode, outputReal );
            cmdList->bindImage( FFT::FFTComputeCol_TextureSourceI_Hashcode, outputImaginary );

            cmdList->bindImage( FFT::FFTComputeCol_TextureTargetR_Hashcode, glarePatternFFT[1] );
            cmdList->bindImage( FFT::FFTComputeCol_TextureTargetI_Hashcode, glarePatternFFT[0] );

            cmdList->prepareAndBindResourceList( pso );

            cmdList->dispatchCompute( 1, FFT_TEXTURE_DIMENSION, 1 );

            cmdList->popEventMarker();
        }
    );
}
