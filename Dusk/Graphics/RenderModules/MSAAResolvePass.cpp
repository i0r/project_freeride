/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "MSAAResolvePass.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/AntiAliasing.generated.h"
#include "Generated/BuiltIn.generated.h"

template<i32 SamplerCount>
const char* GetDepthPassName()
{
    switch ( SamplerCount ) {
    case 2:
        return AntiAliasing::ResolveDepthMSAAx2_Name;
    case 4:
        return AntiAliasing::ResolveDepthMSAAx4_Name;
    case 8:
        return AntiAliasing::ResolveDepthMSAAx8_Name;
    default:
        return "";
    };
}

template<i32 SamplerCount>
const dkChar_t* GetDepthEventName()
{
    switch ( SamplerCount ) {
    case 2:
        return AntiAliasing::ResolveDepthMSAAx2_EventName;
    case 4:
        return AntiAliasing::ResolveDepthMSAAx4_EventName;
    case 8:
        return AntiAliasing::ResolveDepthMSAAx8_EventName;
    default:
        return DUSK_STRING( "" );
    };
}

template<i32 SamplerCount>
PipelineStateCache::ShaderBinding GetDepthPassShaderBinding()
{
    switch ( SamplerCount ) {
    case 2:
        return AntiAliasing::ResolveDepthMSAAx2_ShaderBinding;
    case 4:
        return AntiAliasing::ResolveDepthMSAAx4_ShaderBinding;
    case 8:
        return AntiAliasing::ResolveDepthMSAAx8_ShaderBinding;
    default:
        return AntiAliasing::ResolveDepthMSAAx2_ShaderBinding;
    };
}

template<i32 SamplerCount>
void GetDepthThreadCount( u32& threadCountX, u32& threadCountY, u32& threadCountZ )
{
    switch ( SamplerCount ) {
    case 2:
        threadCountX = AntiAliasing::ResolveDepthMSAAx2_DispatchX;
        threadCountY = AntiAliasing::ResolveDepthMSAAx2_DispatchY;
        threadCountZ = AntiAliasing::ResolveDepthMSAAx2_DispatchZ;
        break;
    case 4:
        threadCountX = AntiAliasing::ResolveDepthMSAAx4_DispatchX;
        threadCountY = AntiAliasing::ResolveDepthMSAAx4_DispatchY;
        threadCountZ = AntiAliasing::ResolveDepthMSAAx4_DispatchZ;
        break;
    case 8:
        threadCountX = AntiAliasing::ResolveDepthMSAAx8_DispatchX;
        threadCountY = AntiAliasing::ResolveDepthMSAAx8_DispatchY;
        threadCountZ = AntiAliasing::ResolveDepthMSAAx8_DispatchZ;
        break;
    default:
        threadCountX = threadCountY = threadCountZ = 1U;
        break;
    };
}

template<i32 SamplerCount>
FGHandle AddResolveDepthMSAARenderPass( FrameGraph& frameGraph,
                                                  FGHandle inputDepthImage )
{
    struct PassData {
        FGHandle InputDepth;
        FGHandle ResolvedDepth;
    };
    
    PassData passData = frameGraph.addRenderPass<PassData>(
        GetDepthPassName<SamplerCount>(),
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.InputDepth = builder.readReadOnlyImage( inputDepthImage );

            ImageDesc* copiedDepthImageDesc;
            passData.ResolvedDepth = builder.copyImage( inputDepthImage, &copiedDepthImageDesc, FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE
                                                        | FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS );
            copiedDepthImageDesc->bindFlags |= RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            copiedDepthImageDesc->bindFlags |= RESOURCE_BIND_SHADER_RESOURCE;

            // Old APIs don't allow both DepthStencil and UAV at the same.
            copiedDepthImageDesc->bindFlags &= ~RESOURCE_BIND_DEPTH_STENCIL;
            copiedDepthImageDesc->DefaultView.ViewFormat = VIEW_FORMAT_R32_FLOAT;
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* inputDepthTarget = resources->getImage( passData.InputDepth );
            Image* resolveDepthTarget = resources->getImage( passData.ResolvedDepth );

            const CameraData* cameraData = resources->getMainCamera();
            const Viewport* vp = resources->getMainViewport();

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, GetDepthPassShaderBinding<SamplerCount>() );

            cmdList->bindPipelineState( pso );
            cmdList->pushEventMarker( GetDepthEventName<SamplerCount>() );

            // The hashcode are shared between permutations; so we can take any permutation name.
            // In the future, we should remove duplicated hashes to avoid bloating generated metadata headers.
		    cmdList->bindImage( AntiAliasing::ResolveDepthMSAAx2_DepthBuffer_Hashcode, inputDepthTarget );
            cmdList->bindImage( AntiAliasing::ResolveDepthMSAAx2_ResolvedDepthTarget_Hashcode, resolveDepthTarget );

            cmdList->prepareAndBindResourceList();

            u32 dispatchX, dispatchY, dispatchZ;
            GetDepthThreadCount<SamplerCount>( dispatchX, dispatchY, dispatchZ );

            u32 threadCountX = DispatchSize( dispatchX, static_cast< u32 >( vp->Width * cameraData->imageQuality ) );
            u32 threadCountY = DispatchSize( dispatchY, static_cast< u32 >( vp->Width * cameraData->imageQuality ) );
            cmdList->dispatchCompute( threadCountX, threadCountY, dispatchZ );
            cmdList->popEventMarker();
        }
    );

    return passData.ResolvedDepth;
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
FGHandle AddResolveMSAARenderPass( FrameGraph& frameGraph,
                                             FGHandle inputImage,
                                             FGHandle inputVelocityImage,
                                             FGHandle inputDepthImage )
{
    struct PassData
    {
        FGHandle Input;
        FGHandle InputDepth;
        FGHandle InputVelocity;
        FGHandle InputLastFrame;
        FGHandle Output;
        FGHandle PerPassBuffer;
        FGHandle AutoExposureBuffer;
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
		    cmdList->bindImage( AntiAliasing::ResolveMSAAx2_DepthBuffer_Hashcode, inputDepthTarget );

            if ( UseTemporalAA ) {
                cmdList->bindImage( AntiAliasing::ResolveMSAAx2WithTAA_VelocityTexture_Hashcode, inputVelocityTarget );
                cmdList->bindImage( AntiAliasing::ResolveMSAAx2WithTAA_LastFrameInputTexture_Hashcode, inputLastFrame );
            }

            cmdList->bindBuffer( AntiAliasing::ResolveMSAAx2_AutoExposureBuffer_Hashcode, autoExposureBuffer );

            cmdList->prepareAndBindResourceList();

            auto& PassRuntimeProperties = AntiAliasing::ResolveMSAAx2WithTAAProperties;
            PassRuntimeProperties.InputTargetDimension.x = static_cast< f32 >( vp->Width * cameraData->imageQuality );
            PassRuntimeProperties.InputTargetDimension.y = static_cast< f32 >( vp->Height * cameraData->imageQuality );
            PassRuntimeProperties.FilterSize = 2.0f;
            PassRuntimeProperties.SampleRadius = static_cast< i32 >( ( PassRuntimeProperties.FilterSize / 2.0f ) + 0.499f );

            cmdList->updateBuffer( *passBuffer, &PassRuntimeProperties, sizeof( PassRuntimeProperties ) );

            u32 dispatchX, dispatchY, dispatchZ;
            GetThreadCount<SamplerCount, UseTemporalAA>( dispatchX, dispatchY, dispatchZ );

            u32 threadCountX = DispatchSize( dispatchX, static_cast< u32 >( vp->Width * cameraData->imageQuality ) );
            u32 threadCountY = DispatchSize( dispatchY, static_cast< u32 >( vp->Height * cameraData->imageQuality ) );
            cmdList->dispatchCompute( threadCountX, threadCountY, dispatchZ );
            cmdList->insertComputeBarrier( *outputTarget );
            cmdList->popEventMarker();

            cmdList->transitionImage( *inputTarget, eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE, 0, CommandList::TRANSITION_COMPUTE_TO_GRAPHICS );
            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_UAV, 0, CommandList::TRANSITION_COMPUTE_TO_GRAPHICS );
        }
    );

    return passData.Output;
}

FGHandle AddMSAAResolveRenderPass(
    FrameGraph& frameGraph,
    FGHandle inputImage,
    FGHandle inputVelocityImage,
    FGHandle inputDepthImage,
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
			? AddResolveMSAARenderPass<1, true>( frameGraph, inputImage, inputVelocityImage, inputDepthImage )
			: AddResolveMSAARenderPass<1, false>( frameGraph, inputImage, inputVelocityImage, inputDepthImage );
    }
}

FGHandle AddCheapMSAAResolveRenderPass( FrameGraph& frameGraph, FGHandle inputImage, const u32 sampleCount )
{
    struct PassData {
        FGHandle Input;
        FGHandle Output;
    };

    PassData passData = frameGraph.addRenderPass<PassData>(
        "AntiAliasing::CheapMSAAResolve",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.Input = builder.readReadOnlyImage( inputImage );

            passData.Output = builder.copyImage( inputImage, nullptr, FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE
                                                 | FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* input = resources->getImage( passData.Input );
            Image* output = resources->getImage( passData.Output );

            cmdList->pushEventMarker( DUSK_STRING( "AntiAliasing::CheapMSAAResolve" ) );
            cmdList->resolveImage( *input, *output );
            cmdList->popEventMarker();
        }
    );

    return passData.Output;
}

FGHandle AddMSAADepthResolveRenderPass( FrameGraph& frameGraph, FGHandle inputDepthImage, const u32 sampleCount )
{
    switch ( sampleCount ) {
    case 2:
        return AddResolveDepthMSAARenderPass<2>( frameGraph, inputDepthImage );
    case 4:
        return AddResolveDepthMSAARenderPass<4>( frameGraph, inputDepthImage );
    case 8:
        return AddResolveDepthMSAARenderPass<8>( frameGraph, inputDepthImage );
    default:
        return AddResolveDepthMSAARenderPass<1>( frameGraph, inputDepthImage );
    }
}

FGHandle AddSSAAResolveRenderPass( FrameGraph& frameGraph, FGHandle resolvedInput, bool isDepth )
{
	constexpr PipelineStateDesc DefaultPipelineStateDesc = PipelineStateDesc(
		PipelineStateDesc::GRAPHICS,
		ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		DepthStencilStateDesc(),
		RasterizerStateDesc( CULL_MODE_BACK ),
		RenderingHelpers::BS_Disabled,
		FramebufferLayoutDesc(
			FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT )
		),
		{ { RenderingHelpers::S_BilinearClampEdge }, 1 }
	);

	struct PassData
	{
		FGHandle Input;
		FGHandle Output;
    };

    PassData& downscaleData = frameGraph.addRenderPass<PassData>(
        AntiAliasing::ResolveSSAA_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.Input = builder.readReadOnlyImage( resolvedInput );

			ImageDesc* copiedDepthImageDesc;
			passData.Output = builder.copyImage( resolvedInput, &copiedDepthImageDesc, FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE
														| FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS_ONE );

            if ( isDepth ) {
                copiedDepthImageDesc->bindFlags |= RESOURCE_BIND_RENDER_TARGET_VIEW;
                copiedDepthImageDesc->bindFlags |= RESOURCE_BIND_SHADER_RESOURCE;

                // Old APIs don't allow both DepthStencil and UAV at the same.
                copiedDepthImageDesc->bindFlags &= ~RESOURCE_BIND_DEPTH_STENCIL;
                copiedDepthImageDesc->DefaultView.ViewFormat = VIEW_FORMAT_R32_FLOAT;
            }
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			Image* output = resources->getImage( passData.Output );
            Image* input = resources->getImage( passData.Input );

            PipelineState* pso = psoCache->getOrCreatePipelineState( DefaultPipelineStateDesc, AntiAliasing::ResolveSSAA_ShaderBinding );

            cmdList->pushEventMarker( AntiAliasing::ResolveSSAA_EventName );
            cmdList->bindPipelineState( pso );

            cmdList->setViewport( *resources->getMainViewport() );

            // Bind resources
			cmdList->bindImage( AntiAliasing::ResolveSSAA_ResolvedTargetInput_Hashcode, input );

            cmdList->prepareAndBindResourceList();

            FramebufferAttachment fboAttachments[2] = { FramebufferAttachment( output ) };
			cmdList->setupFramebuffer( fboAttachments );

            cmdList->draw( 3, 1 );

            cmdList->popEventMarker();
        }
    );

	return downscaleData.Output;
}
