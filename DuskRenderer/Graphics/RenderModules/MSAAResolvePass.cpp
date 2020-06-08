/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "PresentRenderPass.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/AntiAliasing.generated.h"
#include "Generated/BuiltIn.generated.h"

// MSAA off and TAA on.
ResHandle_t AddTAAResolveRenderPass( FrameGraph& frameGraph,
                                     ResHandle_t inputImage,
                                     ResHandle_t inputVelocityImage,
                                     ResHandle_t inputDepthImage )
{
    struct PassData {
        ResHandle_t Input;
        ResHandle_t InputDepth;
        ResHandle_t InputVelocity;
        ResHandle_t InputLastFrame;
        ResHandle_t Output;
        ResHandle_t PerPassBuffer;
        ResHandle_t AutoExposureBuffer;
    };

    PassData passData = frameGraph.addRenderPass<PassData>(
        AntiAliasing::ResolveTAA_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.Input = builder.readReadOnlyImage( inputImage );
            passData.InputVelocity = builder.readReadOnlyImage( inputVelocityImage );
            passData.InputDepth = builder.readReadOnlyImage( inputDepthImage );
            passData.InputLastFrame = builder.retrieveLastFrameImage();

            ImageDesc* copiedImageDesc;
            passData.Output = builder.copyImage( inputImage, &copiedImageDesc, FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS );

            // Toggle the UAV flag (since this is the output of our compute pass).
            copiedImageDesc->bindFlags |= RESOURCE_BIND_UNORDERED_ACCESS_VIEW;

            // PerPass Buffer
            BufferDesc bufferDesc;
            bufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            bufferDesc.SizeInBytes = sizeof( AntiAliasing::ResolveTAARuntimeProperties );
            bufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passData.PerPassBuffer = builder.allocateBuffer( bufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );

            // Persistent AutoExposure Buffer
            passData.AutoExposureBuffer = builder.retrievePersistentBuffer( DUSK_STRING_HASH( "AutoExposure/ReadBuffer" ) );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );

            Image* inputTarget = resources->getImage( passData.Input );
            Image* inputVelocityTarget = resources->getImage( passData.InputVelocity );
            Image* inputDepthTarget = resources->getImage( passData.InputDepth );
            Image* inputLastFrame = resources->getImage( passData.InputLastFrame );

            Image* outputTarget = resources->getImage( passData.Output );
            Buffer* autoExposureBuffer = resources->getPersistentBuffer( passData.AutoExposureBuffer );

            const CameraData* cameraData = resources->getMainCamera();
            const Viewport* vp = resources->getMainViewport();

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, AntiAliasing::ResolveTAA_ShaderBinding );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( AntiAliasing::ResolveTAA_EventName );

            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_UAV );
            cmdList->transitionImage( *inputTarget, eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE, 0, CommandList::TRANSITION_GRAPHICS_TO_COMPUTE );

            // Bind resources
            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );
            cmdList->bindBuffer( AntiAliasing::ResolveTAA_AutoExposureBuffer_Hashcode, autoExposureBuffer );
            cmdList->bindImage( AntiAliasing::ResolveTAA_TextureInput_Hashcode, inputTarget );
            cmdList->bindImage( AntiAliasing::ResolveTAA_VelocityTexture_Hashcode, inputVelocityTarget );
            cmdList->bindImage( AntiAliasing::ResolveTAA_DepthBuffer_Hashcode, inputDepthTarget );
            cmdList->bindImage( AntiAliasing::ResolveTAA_LastFrameInputTexture_Hashcode, inputLastFrame );
            cmdList->bindImage( AntiAliasing::ResolveTAA_ResolvedTarget_Hashcode, outputTarget );

            cmdList->prepareAndBindResourceList( pso );

            AntiAliasing::ResolveTAAProperties.InputTargetDimension.x = static_cast< f32 >( vp->Width );
            AntiAliasing::ResolveTAAProperties.InputTargetDimension.y = static_cast< f32 >( vp->Height );
            AntiAliasing::ResolveTAAProperties.FilterSize = 2.0f;
            AntiAliasing::ResolveTAAProperties.SampleRadius = static_cast< i32 >( ( AntiAliasing::ResolveTAAProperties.FilterSize / 2.0f ) + 0.499f );

            cmdList->updateBuffer( *passBuffer, &AntiAliasing::ResolveTAAProperties, sizeof( AntiAliasing::ResolveTAARuntimeProperties ) );

            u32 threadCountX = static_cast<u32>( vp->Width * cameraData->imageQuality ) / AntiAliasing::ResolveTAA_DispatchX;
            u32 threadCountY = static_cast<u32>( vp->Height * cameraData->imageQuality ) / AntiAliasing::ResolveTAA_DispatchY;

            cmdList->dispatchCompute( threadCountX, threadCountY, AntiAliasing::ResolveTAA_DispatchZ );

            cmdList->insertComputeBarrier( *outputTarget );

            cmdList->popEventMarker();

            cmdList->transitionImage( *inputTarget, eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE, 0, CommandList::TRANSITION_COMPUTE_TO_GRAPHICS );
            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_UAV, 0, CommandList::TRANSITION_COMPUTE_TO_GRAPHICS );
        }
    );

    return passData.Output;
}

template<i32 SamplerCount, bool UseTemporalAA>
const char* GetPassName()
{
    switch ( SamplerCount ) {
    case 2:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx2WithTAA_Name : AntiAliasing::ResolveMSAAx2_Name;
    case 4:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx4WithTAA_Name : AntiAliasing::ResolveMSAAx4_Name;
    case 8:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx8WithTAA_Name : AntiAliasing::ResolveMSAAx8_Name;
    default:
        return AntiAliasing::ResolveTAA_Name;
    };
}

template<i32 SamplerCount, bool UseTemporalAA>
const dkChar_t* GetEventName()
{
    switch ( SamplerCount ) {
    case 2:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx2WithTAA_EventName : AntiAliasing::ResolveMSAAx2_EventName;
    case 4:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx4WithTAA_EventName : AntiAliasing::ResolveMSAAx4_EventName;
    case 8:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx8WithTAA_EventName : AntiAliasing::ResolveMSAAx8_EventName;
    default:
        return AntiAliasing::ResolveTAA_EventName;
    };
}

template<i32 SamplerCount, bool UseTemporalAA>
PipelineStateCache::ShaderBinding GetPassShaderBinding()
{
    switch ( SamplerCount ) {
    case 2:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx2WithTAA_ShaderBinding : AntiAliasing::ResolveMSAAx2_ShaderBinding;
    case 4:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx4WithTAA_ShaderBinding : AntiAliasing::ResolveMSAAx4_ShaderBinding;
    case 8:
        return ( UseTemporalAA ) ? AntiAliasing::ResolveMSAAx8WithTAA_ShaderBinding : AntiAliasing::ResolveMSAAx8_ShaderBinding;
    default:
        return AntiAliasing::ResolveTAA_ShaderBinding;
    };
}

template<i32 SamplerCount, bool UseTemporalAA>
void GetThreadCount( u32& threadCountX, u32& threadCountY, u32& threadCountZ )
{
    switch ( SamplerCount ) {
    case 2:
        if ( UseTemporalAA ) {
            threadCountX = AntiAliasing::ResolveMSAAx2WithTAA_DispatchX;
            threadCountY = AntiAliasing::ResolveMSAAx2WithTAA_DispatchY;
            threadCountZ = AntiAliasing::ResolveMSAAx2WithTAA_DispatchZ;
        } else {
            threadCountX = AntiAliasing::ResolveMSAAx2_DispatchX;
            threadCountY = AntiAliasing::ResolveMSAAx2_DispatchY;
            threadCountZ = AntiAliasing::ResolveMSAAx2_DispatchZ;
        }
        break;
    case 4:
        if ( UseTemporalAA ) {
            threadCountX = AntiAliasing::ResolveMSAAx4WithTAA_DispatchX;
            threadCountY = AntiAliasing::ResolveMSAAx4WithTAA_DispatchY;
            threadCountZ = AntiAliasing::ResolveMSAAx4WithTAA_DispatchZ;
        } else {
            threadCountX = AntiAliasing::ResolveMSAAx4_DispatchX;
            threadCountY = AntiAliasing::ResolveMSAAx4_DispatchY;
            threadCountZ = AntiAliasing::ResolveMSAAx4_DispatchZ;
        }
        break;
    case 8:
        if ( UseTemporalAA ) {
            threadCountX = AntiAliasing::ResolveMSAAx8WithTAA_DispatchX;
            threadCountY = AntiAliasing::ResolveMSAAx8WithTAA_DispatchY;
            threadCountZ = AntiAliasing::ResolveMSAAx8WithTAA_DispatchZ;
        } else {
            threadCountX = AntiAliasing::ResolveMSAAx8_DispatchX;
            threadCountY = AntiAliasing::ResolveMSAAx8_DispatchY;
            threadCountZ = AntiAliasing::ResolveMSAAx8_DispatchZ;
        }
        break;
    default:
        threadCountX = threadCountY = threadCountZ = 1U;
        break;
    };
}

template<i32 SamplerCount, bool UseTemporalAA>
ResHandle_t AddResolveMSAARenderPass( FrameGraph& frameGraph,
                                     ResHandle_t inputImage,
                                     ResHandle_t inputVelocityImage,
                                     ResHandle_t inputDepthImage )
{
    struct PassData {
        ResHandle_t Input;
        ResHandle_t InputDepth;
        ResHandle_t InputVelocity;
        ResHandle_t InputLastFrame;
        ResHandle_t Output;
        ResHandle_t PerPassBuffer;
        ResHandle_t AutoExposureBuffer;
    };
 
    PassData passData = frameGraph.addRenderPass<PassData>(
        GetPassName<SamplerCount, UseTemporalAA>(),
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.Input = builder.readImage( inputImage );

            passData.InputVelocity = builder.readReadOnlyImage( inputVelocityImage );
            passData.InputDepth = builder.readReadOnlyImage( inputDepthImage );
            passData.InputLastFrame = builder.retrieveLastFrameImage();

            ImageDesc* copiedImageDesc;
            passData.Output = builder.copyImage( inputImage, &copiedImageDesc, FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE
                                                                             | FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS );
            
            copiedImageDesc->bindFlags |= RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            
            BufferDesc bufferDesc;
            bufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            bufferDesc.SizeInBytes = sizeof( AntiAliasing::ResolveMSAAx2WithTAAProperties );
            bufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passData.PerPassBuffer = builder.allocateBuffer( bufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );

            passData.AutoExposureBuffer = builder.retrievePersistentBuffer( DUSK_STRING_HASH( "AutoExposure/ReadBuffer" ) );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
            Image* inputTarget = resources->getImage( passData.Input );
            Image* inputVelocityTarget = resources->getImage( passData.InputVelocity );
            Image* inputDepthTarget = resources->getImage( passData.InputDepth );
            Image* inputLastFrame = resources->getPersitentImage( passData.InputLastFrame );
            Image* outputTarget = resources->getImage( passData.Output );
            Buffer* autoExposureBuffer = resources->getPersistentBuffer( passData.AutoExposureBuffer );
            
            const CameraData* cameraData = resources->getMainCamera();
            const Viewport* vp = resources->getMainViewport();
            
            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, GetPassShaderBinding<SamplerCount, UseTemporalAA>() );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( GetEventName<SamplerCount, UseTemporalAA>() );

            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_UAV );
            cmdList->transitionImage( *inputTarget, eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE, 0, CommandList::TRANSITION_GRAPHICS_TO_COMPUTE );

            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

            // The hashcode are shared between permutations; so we can take any permutation name.
            // In the future, we should remove duplicated hashes to avoid bloating generated metadata headers.
            cmdList->bindImage( AntiAliasing::ResolveMSAAx2_TextureInput_Hashcode, inputTarget );
            cmdList->bindImage( AntiAliasing::ResolveMSAAx2_ResolvedTarget_Hashcode, outputTarget );

            if ( UseTemporalAA ) {
                cmdList->bindImage( AntiAliasing::ResolveMSAAx2WithTAA_VelocityTexture_Hashcode, inputVelocityTarget );
                cmdList->bindImage( AntiAliasing::ResolveMSAAx2WithTAA_DepthBuffer_Hashcode, inputDepthTarget );
                cmdList->bindImage( AntiAliasing::ResolveMSAAx2WithTAA_LastFrameInputTexture_Hashcode, inputLastFrame );
            }

            cmdList->bindBuffer( AntiAliasing::ResolveMSAAx2_AutoExposureBuffer_Hashcode, autoExposureBuffer );

            cmdList->prepareAndBindResourceList( pso );

            auto& PassRuntimeProperties = AntiAliasing::ResolveMSAAx2WithTAAProperties;
            PassRuntimeProperties.InputTargetDimension.x = static_cast< f32 >( vp->Width * cameraData->imageQuality );
            PassRuntimeProperties.InputTargetDimension.y = static_cast< f32 >( vp->Height * cameraData->imageQuality );
            PassRuntimeProperties.FilterSize = 2.0f;
            PassRuntimeProperties.SampleRadius = static_cast< i32 >( ( PassRuntimeProperties.FilterSize / 2.0f ) + 0.499f );

            cmdList->updateBuffer( *passBuffer, & PassRuntimeProperties, sizeof( PassRuntimeProperties ) );

            u32 dispatchX, dispatchY, dispatchZ;
            GetThreadCount<SamplerCount, UseTemporalAA>( dispatchX, dispatchY, dispatchZ );

            u32 threadCountX = static_cast<u32>( ( vp->Width * cameraData->imageQuality ) / dispatchX );
            u32 threadCountY = static_cast<u32>( ( vp->Height * cameraData->imageQuality ) / dispatchY );
            cmdList->dispatchCompute( threadCountX, threadCountY, dispatchZ );
            cmdList->insertComputeBarrier( *outputTarget );
            cmdList->popEventMarker();

            cmdList->transitionImage( *inputTarget, eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE, 0, CommandList::TRANSITION_COMPUTE_TO_GRAPHICS );
            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_UAV, 0, CommandList::TRANSITION_COMPUTE_TO_GRAPHICS );
        }
    );

    return passData.Output;
}

ResHandle_t AddMSAAResolveRenderPass(
    FrameGraph& frameGraph,
    ResHandle_t inputImage,
    ResHandle_t inputVelocityImage,
    ResHandle_t inputDepthImage,
    const u32   sampleCount,
    const bool  enableTAA
)
{
    switch ( sampleCount ) {
    case 2:
        return ( enableTAA )
            ? AddResolveMSAARenderPass<2, true>( frameGraph, inputImage, inputVelocityImage, inputDepthImage )
            : AddResolveMSAARenderPass<2, false>( frameGraph, inputImage, inputVelocityImage, inputDepthImage );
    case 4:
        return ( enableTAA )
            ? AddResolveMSAARenderPass<4, true>( frameGraph, inputImage, inputVelocityImage, inputDepthImage )
            : AddResolveMSAARenderPass<4, false>( frameGraph, inputImage, inputVelocityImage, inputDepthImage );
    case 8:
        return ( enableTAA )
            ? AddResolveMSAARenderPass<8, true>( frameGraph, inputImage, inputVelocityImage, inputDepthImage )
            : AddResolveMSAARenderPass<8, false>( frameGraph, inputImage, inputVelocityImage, inputDepthImage );
    default:
        return ( enableTAA ) 
            ? AddTAAResolveRenderPass( frameGraph, inputImage, inputVelocityImage, inputDepthImage ) 
            : inputImage;
    }
}

ResHandle_t AddSSAAResolveRenderPass( FrameGraph& frameGraph, ResHandle_t inputImage )
{
    constexpr PipelineStateDesc DefaultPipelineStateDesc = PipelineStateDesc(
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc( CULL_MODE_BACK ),
        RenderingHelpers::BS_Disabled,
        FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT ) ),
        { { RenderingHelpers::S_BilinearClampEdge }, 1 }
    );

    struct PassData {
        ResHandle_t Input;
        ResHandle_t Output;
    };

    PassData& downscaleData = frameGraph.addRenderPass<PassData>(
        BuiltIn::CopyImagePass_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.Input = builder.readReadOnlyImage( inputImage );

            ImageDesc outputDesc;
            outputDesc.dimension = ImageDesc::DIMENSION_2D;
            outputDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            outputDesc.usage = RESOURCE_USAGE_DEFAULT;
            outputDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;

            passData.Output = builder.allocateImage( outputDesc, FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS_ONE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* output = resources->getImage( passData.Output );
            Image* input = resources->getImage( passData.Input );

            PipelineState* pso = psoCache->getOrCreatePipelineState( DefaultPipelineStateDesc, BuiltIn::CopyImagePass_ShaderBinding );

            cmdList->pushEventMarker( BuiltIn::CopyImagePass_EventName );
            cmdList->bindPipelineState( pso );

            cmdList->setViewport( *resources->getMainViewport() );

            // Bind resources
            cmdList->bindImage( BuiltIn::CopyImagePass_InputRenderTarget_Hashcode, input );

            cmdList->prepareAndBindResourceList( pso );
            cmdList->setupFramebuffer( &output );

            cmdList->draw( 3, 1 );

            cmdList->popEventMarker();
        }
    );

    return downscaleData.Output;
}
