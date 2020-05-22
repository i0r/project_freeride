/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "FinalPostFxRenderPass.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "PostEffects.generated.h"

ResHandle_t AddFinalPostFxRenderPass( FrameGraph& frameGraph, ResHandle_t input, ResHandle_t bloomInput, ResHandle_t glareInput )
{
    struct PassData {
        ResHandle_t input;
        ResHandle_t output;

        ResHandle_t bloomRtInput;
        ResHandle_t glareRtInput;

        ResHandle_t autoExposureBuffer;
        ResHandle_t PerPassBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        PostEffects::Default_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.input = builder.readImage( input );

            passData.bloomRtInput = builder.readImage( bloomInput );
            passData.glareRtInput = builder.readImage( glareInput );

            ImageDesc outputDesc;
            outputDesc.dimension = ImageDesc::DIMENSION_2D;
            outputDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            outputDesc.usage = RESOURCE_USAGE_DEFAULT;
            outputDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;

            passData.output = builder.allocateImage( outputDesc, FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS_ONE );

            // PerPass Buffer
            BufferDesc bufferDesc;
            bufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            bufferDesc.SizeInBytes = sizeof( PostEffects::DefaultRuntimeProperties );
            bufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passData.PerPassBuffer = builder.allocateBuffer( bufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );

            passData.autoExposureBuffer = builder.retrievePersistentBuffer( DUSK_STRING_HASH( "AutoExposure/ReadBuffer" ) );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            const Viewport* vp = resources->getMainViewport();

            Image* outputTarget = resources->getImage( passData.output );
            Image* inputTarget = resources->getImage( passData.input );
            Image* bloomTarget = resources->getImage( passData.bloomRtInput );
            Image* glareTarget = resources->getImage( passData.glareRtInput );
            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* autoExposureBuffer = resources->getPersistentBuffer( passData.autoExposureBuffer );

            PipelineState* passPipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, PostEffects::Default_ShaderBinding );

            cmdList->pushEventMarker( PostEffects::Default_EventName );
            cmdList->bindPipelineState( passPipelineState );

            cmdList->bindImage( PostEffects::Default_InputRenderTarget_Hashcode, inputTarget );
            cmdList->bindImage( PostEffects::Default_BloomRenderTarget_Hashcode, bloomTarget );
            cmdList->bindImage( PostEffects::Default_GlareRenderTarget_Hashcode, glareTarget );
            cmdList->bindImage( PostEffects::Default_OutputRenderTarget_Hashcode, outputTarget );
            cmdList->bindBuffer( PostEffects::Default_AutoExposureBuffer_Hashcode, autoExposureBuffer );

            cmdList->updateBuffer( *passBuffer, &PostEffects::DefaultProperties, sizeof( PostEffects::DefaultRuntimeProperties ) );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );
            
            cmdList->prepareAndBindResourceList( passPipelineState );

            cmdList->dispatchCompute( vp->Width / PostEffects::Default_DispatchX, vp->Height / PostEffects::Default_DispatchY, PostEffects::Default_DispatchZ );
            cmdList->insertComputeBarrier( *outputTarget );

            cmdList->popEventMarker();
        }
    );

    return passData.output;
}
