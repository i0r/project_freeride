/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "FrameCompositionModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/PostEffects.generated.h"

#include "AutomaticExposure.h"

FrameCompositionModule::FrameCompositionModule()
    : colorGradingLUT( nullptr )
{

}

FrameCompositionModule::~FrameCompositionModule()
{
    colorGradingLUT = nullptr;
}

FGHandle FrameCompositionModule::addFrameCompositionPass( FrameGraph& frameGraph, FGHandle input, FGHandle glareInput )
{
    struct PassData {
        FGHandle input;
        FGHandle output;

        FGHandle bloomRtInput;
        FGHandle glareRtInput;

        FGHandle autoExposureBuffer;
        FGHandle PerPassBuffer;
        FGHandle PerViewBuffer;
    };

    constexpr PipelineStateDesc CompositionPipelineDesc = PipelineStateDesc(
        PipelineStateDesc::COMPUTE,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc(),
        BlendStateDesc(),
        FramebufferLayoutDesc(),
        { { RenderingHelpers::S_BilinearClampEdge }, 1 }
    );

    PassData& passData = frameGraph.addRenderPass<PassData>(
        PostEffects::Default_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.input = builder.readReadOnlyImage( input );
            passData.glareRtInput = builder.readReadOnlyImage( glareInput );

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

            passData.autoExposureBuffer = builder.retrievePersistentBuffer( AutomaticExposureModule::PERSISTENT_BUFFER_HASHCODE_READ );
            passData.PerViewBuffer = builder.retrievePerViewBuffer();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            DUSK_GPU_PROFILE_SCOPED( *cmdList, PostEffects::Default_Name );

            const Viewport* vp = resources->getMainViewport();

            Image* outputTarget = resources->getImage( passData.output );
            Image* inputTarget = resources->getImage( passData.input );
            Image* glareTarget = resources->getImage( passData.glareRtInput );

            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* viewBuffer = resources->getBuffer( passData.PerViewBuffer );
            Buffer* autoExposureBuffer = resources->getPersistentBuffer( passData.autoExposureBuffer );

            PipelineState* passPipelineState = psoCache->getOrCreatePipelineState( CompositionPipelineDesc, PostEffects::Default_ShaderBinding );

            cmdList->pushEventMarker( PostEffects::Default_EventName );
            cmdList->bindPipelineState( passPipelineState );

            cmdList->bindImage( PostEffects::Default_InputRenderTarget_Hashcode, inputTarget );
            cmdList->bindImage( PostEffects::Default_GlareRenderTarget_Hashcode, glareTarget );
            cmdList->bindImage( PostEffects::Default_OutputRenderTarget_Hashcode, outputTarget );
            cmdList->bindImage( PostEffects::Default_ColorGradingLUT_Hashcode, colorGradingLUT );
            cmdList->bindBuffer( PostEffects::Default_AutoExposureBuffer_Hashcode, autoExposureBuffer );
            cmdList->bindBuffer( PerViewBufferHashcode, viewBuffer );
            
            cmdList->updateBuffer( *passBuffer, &PostEffects::DefaultProperties, sizeof( PostEffects::DefaultRuntimeProperties ) );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );
            
            cmdList->prepareAndBindResourceList();

            u32 ThreadGroupX = DispatchSize( PostEffects::Default_DispatchX, vp->Width );
            u32 ThreadGroupY = DispatchSize( PostEffects::Default_DispatchY, vp->Height );
            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, PostEffects::Default_DispatchZ );
            cmdList->insertComputeBarrier( *outputTarget );

            cmdList->popEventMarker();
        }
    );

    return passData.output;
}

void FrameCompositionModule::loadCachedResources( GraphicsAssetCache& graphicsAssetCache )
{
    colorGradingLUT = graphicsAssetCache.getImage( DUSK_STRING( "GameData/textures/ColorGrading/LUT_Default.dds" ) );
}
