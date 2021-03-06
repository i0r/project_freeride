/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "ScreenSpaceReflectionModule.h"

#include "Graphics/FrameGraph.h"
#include "Graphics/ShaderCache.h"
#include "Graphics/GraphicsAssetCache.h"
#include "Graphics/ShaderHeaders/Light.h"
#include "Graphics/RenderModules/Generated/DepthPyramid.generated.h"
#include "Graphics/RenderModules/Generated/SSR.generated.h"

#include "Maths/Helpers.h"

SSRModule::SSRModule()
	: blueNoise( nullptr )
	, brdfDfgLut( nullptr )
{

}

SSRModule::~SSRModule()
{
	blueNoise = nullptr;
	brdfDfgLut = nullptr;
}

SSRModule::HiZResult SSRModule::computeHiZMips( FrameGraph& frameGraph, FGHandle resolvedDepthBuffer, const u32 width, const u32 height )
{
	struct PassData {
		FGHandle	PerPassBuffer;
		FGHandle	DepthBufferMip0;
		FGHandle	DepthMips[SSR_MAX_MIP_LEVEL];
		i32         HiZMipCount;
		u32			Mip0Width;
		u32			Mip0Height;
		FGHandle	DepthMipChain;
	};

	PassData data = frameGraph.addRenderPass<PassData>(
		DepthPyramid::DepthDownsample_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			// Per pass buffer.
			BufferDesc perPassDesc;
			perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassDesc.SizeInBytes = sizeof( DepthPyramid::DepthDownsampleRuntimeProperties );
			perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;

			passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );

			// Compute the hi-z mipchain length and allocate each level.
			passData.DepthBufferMip0 = builder.readReadOnlyImage( resolvedDepthBuffer );
			passData.HiZMipCount = 0u;
			passData.Mip0Width = width;
			passData.Mip0Height = height;

			ImageDesc hiZMipDesc;
			hiZMipDesc.dimension = ImageDesc::DIMENSION_2D;
			hiZMipDesc.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
			hiZMipDesc.format = VIEW_FORMAT_R32_FLOAT;
			hiZMipDesc.usage = RESOURCE_USAGE_DEFAULT;

			u32 mipWidth = width >> 1;
			u32 mipHeight = height >> 1;
			while ( mipWidth > 1 && mipHeight> 1 ) {
				hiZMipDesc.width = mipWidth;
				hiZMipDesc.height = mipHeight;
				
				passData.DepthMips[passData.HiZMipCount] = builder.allocateImage( hiZMipDesc );
				passData.HiZMipCount++;

				mipWidth >>= 1;
				mipHeight >>= 1;
			}
#ifdef DUSK_D3D11
            // We must merge the mips together since SM5/D3D11 does not support explicit array binding in shaders.
            ImageDesc mergedDesc;
			mergedDesc.dimension = ImageDesc::DIMENSION_2D;
			mergedDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
			mergedDesc.format = VIEW_FORMAT_R32_FLOAT;
			mergedDesc.usage = RESOURCE_USAGE_DEFAULT;
			mergedDesc.width = width;
			mergedDesc.height = height;
			mergedDesc.mipCount = passData.HiZMipCount;

            passData.DepthMipChain = builder.allocateImage( mergedDesc );
#endif
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_PointClampEdge );

			// TODO Merge all downsampling step into a single shader
			// Should be doable in SM6.0; might be more tricky in SM5.0
			u32 mipWidth = passData.Mip0Width;
			u32 mipHeight = passData.Mip0Height;

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, DepthPyramid::DepthDownsample_ShaderBinding );
			cmdList->pushEventMarker( DepthPyramid::DepthDownsample_EventName );

			cmdList->bindPipelineState( pipelineState );
			cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

			for ( i32 mipIdx = 0; mipIdx < passData.HiZMipCount; mipIdx++ ) {
				DepthPyramid::DepthDownsampleProperties.TextureSize.x = mipWidth >> 1;
				DepthPyramid::DepthDownsampleProperties.TextureSize.y = mipHeight >> 1;

				cmdList->updateBuffer( *perPassBuffer, &DepthPyramid::DepthDownsampleProperties, sizeof( DepthPyramid::DepthDownsampleRuntimeProperties ) );

				Image* previousMipTarget = ( mipIdx == 0 ) ? resources->getImage( passData.DepthBufferMip0 ) : resources->getImage( passData.DepthMips[mipIdx - 1] );
				Image* mipTarget = resources->getImage( passData.DepthMips[mipIdx] );
				cmdList->bindImage( DepthPyramid::DepthDownsample_DepthBuffer_Hashcode, previousMipTarget );
				cmdList->bindImage( DepthPyramid::DepthDownsample_DownsampledepthMip_Hashcode, mipTarget );

				cmdList->prepareAndBindResourceList();

                u32 ThreadGroupX = DispatchSize( DepthPyramid::DepthDownsample_DispatchX, mipWidth );
                u32 ThreadGroupY = DispatchSize( DepthPyramid::DepthDownsample_DispatchY, mipHeight );
                cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, 1u );

                mipWidth >>= 1;
                mipHeight >>= 1;
			}

			// Replace this with caps ifdef (like the async compute one).
#if DUSK_D3D11
			// We need to merge each individual texture into a single one since we can't use explicit texture array (contrary to Vulkan/D3D12).
			Image* mipChain = resources->getImage( passData.DepthMipChain );
			for ( i32 mipIdx = 0; mipIdx < passData.HiZMipCount; mipIdx++ ) {
				Image* mipTarget = ( mipIdx == 0 ) ? resources->getImage( passData.DepthBufferMip0 ) : resources->getImage( passData.DepthMips[mipIdx - 1] );

				ImageViewDesc srcDesc;
				srcDesc.StartMipIndex = 0u;
                srcDesc.MipCount = 1u;
                srcDesc.StartImageIndex = 0;
                srcDesc.ImageCount = 1u;

				ImageViewDesc dstDesc;
				dstDesc.StartImageIndex = 0u;
                dstDesc.ImageCount = 1u;
				dstDesc.StartMipIndex = mipIdx;
				dstDesc.MipCount = 1u;

				cmdList->copyImage( *mipTarget, *mipChain, srcDesc, dstDesc );
			}
#endif

			cmdList->popEventMarker();
		} 
	);

	HiZResult result;
	result.HiZMerged = data.DepthMipChain;

	for ( i32 i = 0; i < data.HiZMipCount; i++ ) {
		result.HiZMips[i] = data.DepthMips[i];
	}

	return result;
}

SSRModule::TraceResult SSRModule::rayTraceHiZ( FrameGraph& frameGraph, FGHandle resolveThinGbuffer, FGHandle colorBuffer, const HiZResult& hiZBuffer, const u32 width, const u32 height )
{
    struct PassData {
        FGHandle	PerPassBuffer;
		FGHandle	HiZBuffer;
        FGHandle	ThinGBuffer;
        FGHandle	ColorBuffer;
		FGHandle	MaskBuffer;
        FGHandle	RayHitBuffer;
    };

	PassData data = frameGraph.addRenderPass<PassData>(
		SSR::HiZTrace_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			// Per pass buffer.
			BufferDesc perPassDesc;
			perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassDesc.SizeInBytes = sizeof( SSR::HiZTraceRuntimeProperties );
            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            
			passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );

			ImageDesc rayHitDesc;
			rayHitDesc.dimension = ImageDesc::DIMENSION_2D;
			rayHitDesc.format = VIEW_FORMAT_R16G16B16A16_FLOAT;
			rayHitDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            rayHitDesc.usage = RESOURCE_USAGE_DEFAULT;
            rayHitDesc.width = width;
            rayHitDesc.height = height;

            passData.RayHitBuffer = builder.allocateImage( rayHitDesc );

			ImageDesc maskDesc;
			maskDesc.dimension = ImageDesc::DIMENSION_2D;
			maskDesc.format = VIEW_FORMAT_R32_FLOAT;
			maskDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
			maskDesc.usage = RESOURCE_USAGE_DEFAULT;
			maskDesc.width = width;
			maskDesc.height = height;

			passData.MaskBuffer = builder.allocateImage( maskDesc );

			passData.ColorBuffer = builder.readReadOnlyImage( colorBuffer );

            passData.ThinGBuffer = builder.readReadOnlyImage( resolveThinGbuffer );
            passData.HiZBuffer = builder.readReadOnlyImage( hiZBuffer.HiZMerged );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
			Image* thinGBuffer = resources->getImage( passData.ThinGBuffer );
			Image* rayHitBuffer = resources->getImage( passData.RayHitBuffer );
			Image* maskBuffer = resources->getImage( passData.MaskBuffer );
            Image* hizBuffer = resources->getImage( passData.HiZBuffer );
            Image* colorBuffer = resources->getImage( passData.ColorBuffer );

			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_PointClampEdge );

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, SSR::HiZTrace_ShaderBinding );
			cmdList->pushEventMarker( SSR::HiZTrace_EventName );

			SSR::HiZTraceProperties.HaltonOffset = dk::maths::HaltonOffset2D();
            SSR::HiZTraceProperties.OutputSize.x = width;
            SSR::HiZTraceProperties.OutputSize.y = height;
            cmdList->updateBuffer( *perPassBuffer, &SSR::HiZTraceProperties, sizeof( SSR::HiZTraceRuntimeProperties ) );

			cmdList->bindPipelineState( pipelineState );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
            cmdList->bindImage( SSR::HiZTrace_RayHitTarget_Hashcode, rayHitBuffer );
			cmdList->bindImage( SSR::HiZTrace_MaskTarget_Hashcode, maskBuffer );
			cmdList->bindImage( SSR::HiZTrace_HiZDepthBuffer_Hashcode, hizBuffer );
			cmdList->bindImage( SSR::HiZTrace_ThinGBuffer_Hashcode, thinGBuffer );
            cmdList->bindImage( SSR::HiZTrace_BlueNoise_Hashcode, blueNoise );
            cmdList->bindImage( SSR::HiZTrace_ColorBuffer_Hashcode, colorBuffer );
            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( DispatchSize( SSR::HiZTrace_DispatchX, width ), DispatchSize( SSR::HiZTrace_DispatchY, height ), SSR::HiZTrace_DispatchZ );

			cmdList->popEventMarker();
		} 
	);


	TraceResult result;
	result.TraceBuffer = data.RayHitBuffer;
	result.MaskBuffer = data.MaskBuffer;
	return result;
}

FGHandle SSRModule::resolveRaytrace( FrameGraph& frameGraph, const SSRModule::TraceResult& traceResult, FGHandle colorBuffer, FGHandle resolvedThinGbuffer, const HiZResult& hiZBuffer, const u32 width, const u32 height )
{
    struct PassData {
        FGHandle	PerPassBuffer;
        FGHandle	ThinGBuffer;
        FGHandle	ColorBuffer;
		FGHandle	HitBuffer;
		FGHandle	HiZBuffer;
		FGHandle	MaskBuffer;
        FGHandle	ResolvedBuffer;
    };

	PassData data = frameGraph.addRenderPass<PassData>(
		SSR::ResolveTrace_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			// Per pass buffer.
			BufferDesc perPassDesc;
			perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassDesc.SizeInBytes = sizeof( SSR::ResolveTraceRuntimeProperties );
            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );

			ImageDesc resolvedTargetDesc;
			resolvedTargetDesc.dimension = ImageDesc::DIMENSION_2D;
			resolvedTargetDesc.format = VIEW_FORMAT_R16G16B16A16_FLOAT;
			resolvedTargetDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
			resolvedTargetDesc.usage = RESOURCE_USAGE_DEFAULT;
			resolvedTargetDesc.width = width;
			resolvedTargetDesc.height = height;

            passData.ResolvedBuffer = builder.allocateImage( resolvedTargetDesc );

            passData.ThinGBuffer = builder.readReadOnlyImage( resolvedThinGbuffer );
            passData.ColorBuffer = builder.readReadOnlyImage( colorBuffer );
			passData.HitBuffer = builder.readReadOnlyImage( traceResult.TraceBuffer );
			passData.MaskBuffer = builder.readReadOnlyImage( traceResult.MaskBuffer );
			passData.HiZBuffer = builder.readReadOnlyImage( hiZBuffer.HiZMerged );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            Image* resolvedBuffer = resources->getImage( passData.ResolvedBuffer );

            Image* thinGBuffer = resources->getImage( passData.ThinGBuffer );
            Image* colorBuffer = resources->getImage( passData.ColorBuffer );
			Image* rayHitBuffer = resources->getImage( passData.HitBuffer );
			Image* maskBuffer = resources->getImage( passData.MaskBuffer );
			Image* hizBuffer = resources->getImage( passData.HiZBuffer );

			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_PointClampEdge );

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, SSR::ResolveTrace_ShaderBinding );
			cmdList->pushEventMarker( SSR::ResolveTrace_EventName );

            cmdList->updateBuffer( *perPassBuffer, &SSR::HiZTraceProperties, sizeof( SSR::ResolveTraceRuntimeProperties ) );

			cmdList->bindPipelineState( pipelineState );
			cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
			//cmdList->bindImage( SSR::ResolveTrace_HiZDepthBuffer_Hashcode, hizBuffer );
            //cmdList->bindImage( SSR::ResolveTrace_ThinGBuffer_Hashcode, thinGBuffer );
            cmdList->bindImage( SSR::ResolveTrace_ColorBuffer_Hashcode, colorBuffer );
			cmdList->bindImage( SSR::ResolveTrace_RayTraceBuffer_Hashcode, rayHitBuffer );
            cmdList->bindImage( SSR::ResolveTrace_ResolvedOutput_Hashcode, resolvedBuffer );
            //cmdList->bindImage( SSR::ResolveTrace_MaskBuffer_Hashcode, maskBuffer );
            //cmdList->bindImage( SSR::ResolveTrace_BlueNoise_Hashcode, blueNoise );
			//cmdList->bindImage( SSR::ResolveTrace_BrdfDfgLut_Hashcode, brdfDfgLut );
            cmdList->prepareAndBindResourceList();

            u32 ThreadGroupX = DispatchSize( SSR::HiZTrace_DispatchX, SSR::HiZTraceProperties.OutputSize.x );
            u32 ThreadGroupY = DispatchSize( SSR::HiZTrace_DispatchY, SSR::HiZTraceProperties.OutputSize.y );
            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, SSR::HiZTrace_DispatchZ );

			cmdList->popEventMarker();
		} 
	);

	return data.ResolvedBuffer;
}

//FGHandle SSRModule::temporalRebuild( FrameGraph& frameGraph, FGHandle rayTraceOutput, FGHandle resolvedOutput, const u32 width, const u32 height )
//{
//    struct PassData {
//        FGHandle	PerPassBuffer;
//        FGHandle	RayTraceOutput;
//        FGHandle	ResolveOutput;
//        FGHandle	PreviousFrameResult;
//        FGHandle	TemporalFrameResult;
//    };
//
//	PassData data = frameGraph.addRenderPass<PassData>(
//		SSR::TemporalRebuild_Name,
//		[&]( FrameGraphBuilder& builder, PassData& passData ) {
//			builder.useAsyncCompute();
//			builder.setUncullablePass();
//
//			// Per pass buffer.
//			BufferDesc perPassDesc;
//			perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
//			perPassDesc.SizeInBytes = sizeof( SSR::TemporalRebuildRuntimeProperties );
//            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;
//            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );
//
//            passData.RayTraceOutput = builder.readReadOnlyImage( rayTraceOutput );
//            passData.ResolveOutput = builder.readReadOnlyImage( resolvedOutput );
//            passData.PreviousFrameResult = builder.retrieveSSRLastFrameImage();
//
//            ImageDesc resolvedTargetDesc;
//            resolvedTargetDesc.dimension = ImageDesc::DIMENSION_2D;
//            resolvedTargetDesc.format = VIEW_FORMAT_R16G16B16A16_FLOAT;
//            resolvedTargetDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
//            resolvedTargetDesc.usage = RESOURCE_USAGE_DEFAULT;
//            resolvedTargetDesc.width = width;
//            resolvedTargetDesc.height = height;
//
//            passData.TemporalFrameResult = builder.allocateImage( resolvedTargetDesc );
//		},
//		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
//			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
//
//            Image* rayTraceBuffer = resources->getImage( passData.RayTraceOutput );
//            Image* resolvedBuffer = resources->getImage( passData.ResolveOutput );
//            Image* previousFrameBuffer = resources->getPersitentImage( passData.PreviousFrameResult );
//            Image* temporalOutput = resources->getImage( passData.TemporalFrameResult );
//
//			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
//			psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );
//
//			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, SSR::TemporalRebuild_ShaderBinding );
//			cmdList->pushEventMarker( SSR::TemporalRebuild_EventName );
//
//			SSR::TemporalRebuildProperties.HaltonOffset = dk::maths::HaltonOffset2D();
//            SSR::TemporalRebuildProperties.OutputSize.x = width;
//            SSR::TemporalRebuildProperties.OutputSize.y = height;
//            cmdList->updateBuffer( *perPassBuffer, &SSR::TemporalRebuildProperties, sizeof( SSR::TemporalRebuildRuntimeProperties ) );
//
//			cmdList->bindPipelineState( pipelineState );
//            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
//            cmdList->bindImage( SSR::TemporalRebuild_RayTraceBuffer_Hashcode, rayTraceBuffer );
//			cmdList->bindImage( SSR::TemporalRebuild_PreviousFrameResult_Hashcode, previousFrameBuffer );
//			cmdList->bindImage( SSR::TemporalRebuild_ResolvedTraceBuffer_Hashcode, resolvedBuffer );
//			cmdList->bindImage( SSR::TemporalRebuild_TemporalResultTarget_Hashcode, temporalOutput );
//            cmdList->prepareAndBindResourceList();
//
//            u32 ThreadGroupX = Max( 1u, SSR::TemporalRebuildProperties.OutputSize.x / SSR::TemporalRebuild_DispatchX );
//            u32 ThreadGroupY = Max( 1u, SSR::TemporalRebuildProperties.OutputSize.y / SSR::TemporalRebuild_DispatchY );
//            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, SSR::TemporalRebuild_DispatchZ);
//
//			cmdList->popEventMarker();
//		} 
//	);
//
//	return data.TemporalFrameResult;
//}
//
//FGHandle SSRModule::combineResult( FrameGraph& frameGraph, FGHandle temporalRebuiltBuffer, FGHandle sceneColor, FGHandle depthBuffer, FGHandle thinGbuffer, const u32 width, const u32 height )
//{
//    struct PassData {
//        FGHandle	PerPassBuffer;
//        FGHandle	RebuiltBuffer;
//        FGHandle	SceneColor;
//        FGHandle	DepthBuffer;
//        FGHandle	ThinGBuffer;
//        FGHandle	CombinedBuffer;
//    };
//
//	PassData data = frameGraph.addRenderPass<PassData>(
//		SSR::Combine_Name,
//		[&]( FrameGraphBuilder& builder, PassData& passData ) {
//			builder.useAsyncCompute();
//			builder.setUncullablePass();
//
//			// Per pass buffer.
//			BufferDesc perPassDesc;
//			perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
//			perPassDesc.SizeInBytes = sizeof( SSR::CombineRuntimeProperties );
//            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;
//            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );
//
//            passData.RebuiltBuffer = builder.readReadOnlyImage( temporalRebuiltBuffer );
//            passData.SceneColor = builder.readReadOnlyImage( sceneColor );
//            passData.DepthBuffer = builder.readReadOnlyImage( depthBuffer );
//            passData.ThinGBuffer = builder.readReadOnlyImage( thinGbuffer );
//
//            ImageDesc combinedTargetDesc;
//            combinedTargetDesc.dimension = ImageDesc::DIMENSION_2D;
//            combinedTargetDesc.format = VIEW_FORMAT_R16G16B16A16_FLOAT;
//            combinedTargetDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
//            combinedTargetDesc.usage = RESOURCE_USAGE_DEFAULT;
//            combinedTargetDesc.width = width;
//            combinedTargetDesc.height = height;
//
//            passData.CombinedBuffer = builder.allocateImage( combinedTargetDesc );
//		},
//		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
//			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
//
//            Image* combinedResult = resources->getImage( passData.CombinedBuffer );
//            Image* temporalRebuiltBuffer = resources->getImage( passData.RebuiltBuffer );
//            Image* sceneColorBuffer = resources->getImage( passData.SceneColor );
//            Image* depthBuffer = resources->getImage( passData.DepthBuffer );
//            Image* thinGBuffer = resources->getImage( passData.ThinGBuffer );
//
//			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
//			psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );
//
//			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, SSR::Combine_ShaderBinding );
//			cmdList->pushEventMarker( SSR::Combine_EventName );
//
//			SSR::CombineProperties.HaltonOffset = dk::maths::HaltonOffset2D();
//            SSR::CombineProperties.OutputSize.x = width;
//            SSR::CombineProperties.OutputSize.y = height;
//            cmdList->updateBuffer( *perPassBuffer, &SSR::CombineProperties, sizeof( SSR::CombineRuntimeProperties ) );
//
//			cmdList->bindPipelineState( pipelineState );
//            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
//            cmdList->bindImage( SSR::Combine_ThinGBuffer_Hashcode, thinGBuffer );
//            cmdList->bindImage( SSR::Combine_BrdfDfgLut_Hashcode, brdfDfgLut );
//            cmdList->bindImage( SSR::Combine_SceneColorBuffer_Hashcode, sceneColorBuffer );
//            cmdList->bindImage( SSR::Combine_SSRCompositionInput_Hashcode, temporalRebuiltBuffer );
//            cmdList->bindImage( SSR::Combine_LinearDepthBuffer_Hashcode, depthBuffer );
//            cmdList->bindImage( SSR::Combine_CombinedResultTarget_Hashcode, combinedResult );
//            cmdList->prepareAndBindResourceList();
//
//            u32 ThreadGroupX = Max( 1u, SSR::CombineProperties.OutputSize.x / SSR::Combine_DispatchX );
//            u32 ThreadGroupY = Max( 1u, SSR::CombineProperties.OutputSize.y / SSR::Combine_DispatchY );
//            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, SSR::Combine_DispatchZ );
//
//			cmdList->popEventMarker();
//		} 
//	);
//
//	return data.CombinedBuffer;
//}

void SSRModule::destroy( RenderDevice& renderDevice )
{

}

void SSRModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    blueNoise = graphicsAssetCache.getImage( DUSK_STRING( "GameData/textures/bluenoise.dds" ) );
    brdfDfgLut = graphicsAssetCache.getImage( DUSK_STRING( "GameData/textures/BRDF_DFG_Default.dds" ) );
}