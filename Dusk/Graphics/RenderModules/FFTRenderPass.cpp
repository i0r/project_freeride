/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "FFTRenderPass.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/FFT.generated.h"
#include "Generated/BuiltIn.generated.h"

// Calculate the downscaled dimension to make an image suitable for FFT compute.
static dkVec2f FitImageDimensionForFFT( const dkVec2f& inputTargetSize )
{
    // Image dimension without FFT padding. We want minimal padding to keep a decent image resolution
    // without any artifact.
    constexpr i32 FFT_TEXTURE_DIMENSION_WITHOUT_BORDER = ( FFT_TEXTURE_DIMENSION - 64 );

    dkVec2f downscaledInput = inputTargetSize;
    f32 downscaleFactor = 1.00f;

    while ( downscaledInput.x > FFT_TEXTURE_DIMENSION_WITHOUT_BORDER 
         || downscaledInput.y > FFT_TEXTURE_DIMENSION_WITHOUT_BORDER ) {
        downscaledInput = ( inputTargetSize * downscaleFactor );
        downscaleFactor -= 0.01f;
    }

    return downscaledInput;
}

FFTPassOutput AddFFTComputePass( FrameGraph& frameGraph, FGHandle input, f32 inputTargetWidth, f32 inputTargetHeight )
{ 
    struct PassData {
        FGHandle outputReal[2];
        FGHandle outputImaginary[2];

        FGHandle inputImage;
    };

    dkVec2f inputTargetSize = dkVec2f( inputTargetWidth, inputTargetHeight );
    dkVec2f downscaledInput = FitImageDimensionForFFT( inputTargetSize );
    
    const bool needRescale = ( inputTargetSize != downscaledInput );

    if ( needRescale ) {
        static constexpr PipelineStateDesc DownscaleWithPaddingDesc = PipelineStateDesc( 
            PipelineStateDesc::GRAPHICS,
            ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            DepthStencilStateDesc(),
            RasterizerStateDesc( CULL_MODE_BACK ),
            RenderingHelpers::BS_Disabled,
            FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::CLEAR, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT ) ),
            { { RenderingHelpers::S_BilinearClampEdge }, 1 }
        );
        
        struct DownscalePassData {
            FGHandle inputImage;
            FGHandle downscaledImage;
        };

        DownscalePassData& downscaleData = frameGraph.addRenderPass<DownscalePassData>(
            BuiltIn::CopyImagePass_Name,
            [&]( FrameGraphBuilder& builder, DownscalePassData& passData ) {
                builder.setUncullablePass();

                ImageDesc imageDesc;
                imageDesc.dimension = ImageDesc::DIMENSION_2D;
                imageDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
                imageDesc.width = FFT_TEXTURE_DIMENSION;
                imageDesc.height = FFT_TEXTURE_DIMENSION;
                imageDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;
                imageDesc.bindFlags = eResourceBind::RESOURCE_BIND_RENDER_TARGET_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;

                passData.downscaledImage = builder.allocateImage( imageDesc );
                passData.inputImage = builder.readReadOnlyImage( input );
            },
            [=]( const DownscalePassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
                Image* output = resources->getImage( passData.downscaledImage );
                Image* input = resources->getImage( passData.inputImage );

                PipelineState* pso = psoCache->getOrCreatePipelineState( DownscaleWithPaddingDesc, BuiltIn::CopyImagePass_ShaderBinding );

                cmdList->pushEventMarker( BuiltIn::CopyImagePass_EventName );
                cmdList->bindPipelineState( pso );

                f32 borderHorizontal = ( FFT_TEXTURE_DIMENSION - downscaledInput.x );
                f32 borderVertical = ( FFT_TEXTURE_DIMENSION - downscaledInput.y );

                Viewport downscaleVp;
                downscaleVp.X = static_cast< i32 >( borderHorizontal / 2 );
                downscaleVp.Y = static_cast< i32 >( borderVertical / 2 );
                downscaleVp.Width = static_cast< i32 >( downscaledInput.x );
                downscaleVp.Height = static_cast< i32 >( downscaledInput.y );
                downscaleVp.MinDepth = 0.0f;
                downscaleVp.MaxDepth = 1.0f;
                cmdList->setViewport( downscaleVp );

                // Bind resources
                cmdList->bindImage( BuiltIn::CopyImagePass_InputRenderTarget_Hashcode, input );

                cmdList->prepareAndBindResourceList();

                FramebufferAttachment fboAttachment( output );
                cmdList->setupFramebuffer( &fboAttachment );

                cmdList->draw( 3, 1 );

                cmdList->popEventMarker();
            }
        );
    
        input = downscaleData.downscaledImage;
    }

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

            for ( i32 i = 0; i < 2; i++ ) {
                passData.outputReal[i] = builder.allocateImage( imageDesc );
                passData.outputImaginary[i] = builder.allocateImage( imageDesc );
            }

            passData.inputImage = builder.readReadOnlyImage( input );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputReal = resources->getImage( passData.outputReal[0] );
            Image* outputImaginary = resources->getImage( passData.outputImaginary[0] );

            Image* input = resources->getImage( passData.inputImage );

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::FFTComputeRow_ShaderBinding );

            cmdList->pushEventMarker( FFT::FFTComputeRow_EventName );
            cmdList->bindPipelineState( pso );

            // Bind resources
            cmdList->bindImage( FFT::FFTComputeRow_TextureSourceR_Hashcode, input );
            cmdList->bindImage( FFT::FFTComputeRow_TextureTargetR_Hashcode, outputReal );
            cmdList->bindImage( FFT::FFTComputeRow_TextureTargetI_Hashcode, outputImaginary );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1, FFT_TEXTURE_DIMENSION, 1 );

            cmdList->popEventMarker();

            Image* outputReal2 = resources->getImage( passData.outputReal[1] );
            Image* outputImaginary2 = resources->getImage( passData.outputImaginary[1] );

            PipelineState* psoCol = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::FFTComputeCol_ShaderBinding );

            cmdList->pushEventMarker( FFT::FFTComputeCol_EventName );
            cmdList->bindPipelineState( psoCol );

            // Bind resources
            cmdList->bindImage( FFT::FFTComputeCol_TextureSourceR_Hashcode, outputReal );
            cmdList->bindImage( FFT::FFTComputeCol_TextureSourceI_Hashcode, outputImaginary );

            cmdList->bindImage( FFT::FFTComputeCol_TextureTargetR_Hashcode, outputReal2 );
            cmdList->bindImage( FFT::FFTComputeCol_TextureTargetI_Hashcode, outputImaginary2 );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1, FFT_TEXTURE_DIMENSION, 1 );

            cmdList->popEventMarker();
        }
    );

    FFTPassOutput output;
    output.RealPart = rowPassData.outputReal[1];
    output.ImaginaryPart = rowPassData.outputImaginary[1];

    return output;
}

FGHandle AddInverseFFTComputePass( FrameGraph& frameGraph, FFTPassOutput& inputInFrequencyDomain, f32 outputTargetWidth, f32 outputTargetHeight )
{
    struct PassDataRow {
        FGHandle inputReal;
        FGHandle inputImaginary;

        FGHandle outputReal;
        FGHandle outputImaginary;
    };
    
    PassDataRow& rowPassData = frameGraph.addRenderPass<PassDataRow>(
        FFT::InverseFFTComputeRow_Name,
        [&]( FrameGraphBuilder& builder, PassDataRow& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            passData.inputReal = builder.readReadOnlyImage( inputInFrequencyDomain.RealPart );
            passData.inputImaginary = builder.readReadOnlyImage( inputInFrequencyDomain.ImaginaryPart );

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
        [=]( const PassDataRow& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* inputReal = resources->getImage( passData.inputReal );
            Image* inputImaginary = resources->getImage( passData.inputImaginary );

            Image* outputReal = resources->getImage( passData.outputReal );
            Image* outputImaginary = resources->getImage( passData.outputImaginary );

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::InverseFFTComputeRow_ShaderBinding );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( FFT::InverseFFTComputeRow_EventName );

            // Bind resources
            cmdList->bindImage( FFT::InverseFFTComputeRow_TextureSourceR_Hashcode, inputReal );
            cmdList->bindImage( FFT::InverseFFTComputeRow_TextureSourceI_Hashcode, inputImaginary );
            cmdList->bindImage( FFT::InverseFFTComputeRow_TextureTargetR_Hashcode, outputReal );
            cmdList->bindImage( FFT::InverseFFTComputeRow_TextureTargetI_Hashcode, outputImaginary );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1, FFT_TEXTURE_DIMENSION, 1 );

            cmdList->popEventMarker();
        }
    );
    
    struct PassDataCol {
        FGHandle inputReal;
        FGHandle inputImaginary;

        FGHandle outputReal;
        FGHandle outputImaginary;
    };
    
    PassDataCol& colPassData = frameGraph.addRenderPass<PassDataCol>(
        FFT::InverseFFTComputeCol_Name,
        [&]( FrameGraphBuilder& builder, PassDataCol& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            passData.inputReal = builder.readReadOnlyImage( rowPassData.outputReal );
            passData.inputImaginary = builder.readReadOnlyImage( rowPassData.outputImaginary );

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
        [=]( const PassDataCol& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* inputReal = resources->getImage( passData.inputReal );
            Image* inputImaginary = resources->getImage( passData.inputImaginary );

            Image* outputReal = resources->getImage( passData.outputReal );
            Image* outputImaginary = resources->getImage( passData.outputImaginary );

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::InverseFFTComputeCol_ShaderBinding );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( FFT::InverseFFTComputeCol_EventName );

            // Bind resources
            cmdList->bindImage( FFT::InverseFFTComputeCol_TextureSourceR_Hashcode, inputReal );
            cmdList->bindImage( FFT::InverseFFTComputeCol_TextureSourceI_Hashcode, inputImaginary );
            cmdList->bindImage( FFT::InverseFFTComputeCol_TextureTargetR_Hashcode, outputReal );
            cmdList->bindImage( FFT::InverseFFTComputeCol_TextureTargetI_Hashcode, outputImaginary );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1, FFT_TEXTURE_DIMENSION, 1 );

            cmdList->popEventMarker();

            pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, FFT::FFTRealShift_ShaderBinding );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( FFT::FFTRealShift_EventName );

            // Bind resources
            cmdList->bindImage( FFT::FFTRealShift_TextureSourceR_Hashcode, outputReal );
            cmdList->bindImage( FFT::FFTRealShift_TextureTargetR_Hashcode, inputReal );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( FFT_TEXTURE_DIMENSION / FFT::FFTRealShift_DispatchX, FFT_TEXTURE_DIMENSION / FFT::FFTRealShift_DispatchY, FFT::FFTRealShift_DispatchZ );

            cmdList->popEventMarker();
        }
    );

    dkVec2f inputTargetSize = dkVec2f( outputTargetWidth, outputTargetHeight );
    dkVec2f downscaledInput = FitImageDimensionForFFT( inputTargetSize );

    const bool needRescale = ( inputTargetSize != downscaledInput );

    // Input target needs to be downscaled to fit the FFT_TEXTURE_DIM.
    if ( needRescale ) {
        static constexpr PipelineStateDesc DownscaleWithPaddingDesc = PipelineStateDesc( 
            PipelineStateDesc::GRAPHICS,
            ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
            DepthStencilStateDesc(),
            RasterizerStateDesc( CULL_MODE_BACK ),
            RenderingHelpers::BS_Disabled,
            FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT ) ),
            { { RenderingHelpers::S_BilinearWrap }, 1 }
        );
        
        struct DownscalePassData {
            FGHandle inputImage;
            FGHandle downscaledImage;
            FGHandle passBuffer;
        };

        f32 borderHorizontal = ( FFT_TEXTURE_DIMENSION - downscaledInput.x ) * 0.5f;
        f32 borderVertical = ( FFT_TEXTURE_DIMENSION - downscaledInput.y ) * 0.5f;

        f32 leftUv = borderHorizontal / FFT_TEXTURE_DIMENSION;
        f32 topUv = borderVertical / FFT_TEXTURE_DIMENSION;
        f32 rightUv = leftUv + ( downscaledInput.x / FFT_TEXTURE_DIMENSION );
        f32 bottomUv = topUv + ( downscaledInput.y / FFT_TEXTURE_DIMENSION );

        dkVec4f BorderOffsets = dkVec4f(
            leftUv,
            topUv,
            rightUv,
            bottomUv
        );

        DownscalePassData& downscaleData = frameGraph.addRenderPass<DownscalePassData>(
            FFT::UpscaleConvolutedFFT_Name,
            [&]( FrameGraphBuilder& builder, DownscalePassData& passData ) {
                builder.setUncullablePass();

                ImageDesc imageDesc;
                imageDesc.dimension = ImageDesc::DIMENSION_2D;
                imageDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
                imageDesc.width = static_cast<u32>( outputTargetWidth );
                imageDesc.height = static_cast< u32 >( outputTargetHeight );
                imageDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;
                imageDesc.bindFlags = eResourceBind::RESOURCE_BIND_RENDER_TARGET_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;

                passData.downscaledImage = builder.allocateImage( imageDesc );
                passData.inputImage = builder.readReadOnlyImage( colPassData.inputReal );

                BufferDesc perPassBuffer;
                perPassBuffer.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
                perPassBuffer.SizeInBytes = sizeof( FFT::UpscaleConvolutedFFTRuntimeProperties );
                perPassBuffer.Usage = RESOURCE_USAGE_DYNAMIC;

                passData.passBuffer = builder.allocateBuffer( perPassBuffer, eShaderStage::SHADER_STAGE_COMPUTE );
            },
            [=]( const DownscalePassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
                Image* output = resources->getImage( passData.downscaledImage );
                Image* input = resources->getImage( passData.inputImage );
                Buffer* perPassBuffer = resources->getBuffer( passData.passBuffer );

                PipelineState* pso = psoCache->getOrCreatePipelineState( DownscaleWithPaddingDesc, FFT::UpscaleConvolutedFFT_ShaderBinding );

                cmdList->pushEventMarker( FFT::UpscaleConvolutedFFT_EventName );
                cmdList->bindPipelineState( pso );

                Viewport downscaleVp;
                downscaleVp.X = 0;
                downscaleVp.Y = 0;
                downscaleVp.Width = static_cast< i32 >( outputTargetWidth );
                downscaleVp.Height = static_cast< i32 >( outputTargetHeight );
                downscaleVp.MinDepth = 0.0f;
                downscaleVp.MaxDepth = 1.0f;
                cmdList->setViewport( downscaleVp );

                // Update PerPass buffer
                FFT::UpscaleConvolutedFFTProperties.BorderOffsets = BorderOffsets;
                cmdList->updateBuffer( *perPassBuffer, &FFT::UpscaleConvolutedFFTProperties, sizeof( FFT::UpscaleConvolutedFFTRuntimeProperties ) );

                // Bind resources
                cmdList->bindImage( FFT::UpscaleConvolutedFFT_InputRenderTarget_Hashcode, input );
                cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

                cmdList->prepareAndBindResourceList();

                FramebufferAttachment fboAttachment( output );
                cmdList->setupFramebuffer( &fboAttachment );

                cmdList->draw( 4, 1 );

                cmdList->popEventMarker();
            }
        );
    
        return downscaleData.downscaledImage;
    }

    return colPassData.inputReal;
}
