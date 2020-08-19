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
{

}

SSRModule::~SSRModule()
{
	blueNoise = nullptr;
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

	PassData HiZCompute = frameGraph.addRenderPass<PassData>(
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

            ImageDesc mergedDesc;
			mergedDesc.dimension = ImageDesc::DIMENSION_2D;
			mergedDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
			mergedDesc.format = VIEW_FORMAT_R32_FLOAT;
			mergedDesc.usage = RESOURCE_USAGE_DEFAULT;
			mergedDesc.width = width;
			mergedDesc.height = height;
			mergedDesc.mipCount = passData.HiZMipCount;

            passData.DepthMipChain = builder.allocateImage( mergedDesc );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_PointClampEdge );

			// TODO Merge all downsampling step into a single shader
			// Should be doable in SM6.0; might be more tricky in SM5.0
			u32 mipWidth = passData.Mip0Width >> 1;
			u32 mipHeight = passData.Mip0Height >> 1;

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, DepthPyramid::DepthDownsample_ShaderBinding );
			cmdList->pushEventMarker( DepthPyramid::DepthDownsample_EventName );

			cmdList->bindPipelineState( pipelineState );
			cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

			for ( i32 mipIdx = 0; mipIdx < passData.HiZMipCount; mipIdx++ ) {
				DepthPyramid::DepthDownsampleProperties.TextureSize.x = mipWidth;
				DepthPyramid::DepthDownsampleProperties.TextureSize.y = mipHeight;

				cmdList->updateBuffer( *perPassBuffer, &DepthPyramid::DepthDownsampleProperties, sizeof( DepthPyramid::DepthDownsampleRuntimeProperties ) );

				Image* previousMipTarget = ( mipIdx == 0 ) ? resources->getImage( passData.DepthBufferMip0 ) : resources->getImage( passData.DepthMips[mipIdx - 1] );
				Image* mipTarget = resources->getImage( passData.DepthMips[mipIdx] );
				cmdList->bindImage( DepthPyramid::DepthDownsample_DepthBuffer_Hashcode, previousMipTarget );
				cmdList->bindImage( DepthPyramid::DepthDownsample_DownsampledepthMip_Hashcode, mipTarget );

				cmdList->prepareAndBindResourceList();

				u32 ThreadGroupX = Max( 1u, mipWidth / DepthPyramid::DepthDownsample_DispatchX );
				u32 ThreadGroupY = Max( 1u, mipHeight / DepthPyramid::DepthDownsample_DispatchY );
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
	result.HiZMerged = HiZCompute.DepthMipChain;

	for ( i32 i = 0; i < HiZCompute.HiZMipCount; i++ ) {
		result.HiZMips[i] = HiZCompute.DepthMips[i];
	}

	return result;
}

void SSRModule::rayTraceHiZ( FrameGraph& frameGraph, FGHandle resolvedDepthBuffer, FGHandle resolveThinGbuffer, const HiZResult& hiZBuffer, const u32 width, const u32 height )
{
    struct PassData {
        FGHandle	PerPassBuffer;
        FGHandle	DepthBuffer;
        FGHandle	ThinGBuffer;
        FGHandle	RayHitBuffer;
        FGHandle	MaskBuffer;
		FGHandle	HiZBuffer;
    };

	PassData HiZCompute = frameGraph.addRenderPass<PassData>(
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

            passData.DepthBuffer = builder.readReadOnlyImage( resolvedDepthBuffer );
            passData.ThinGBuffer = builder.readReadOnlyImage( resolveThinGbuffer );
            passData.HiZBuffer = builder.readReadOnlyImage( hiZBuffer.HiZMerged );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
			Image* thinGBuffer = resources->getImage( passData.ThinGBuffer );
            Image* depthBuffer = resources->getImage( passData.DepthBuffer );
            Image* rayHitBuffer = resources->getImage( passData.RayHitBuffer );
            Image* maskBuffer = resources->getImage( passData.MaskBuffer );
            Image* hizBuffer = resources->getImage( passData.HiZBuffer );

			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

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
            cmdList->bindImage( SSR::HiZTrace_DepthBuffer_Hashcode, depthBuffer );
            cmdList->bindImage( SSR::HiZTrace_ThinGBuffer_Hashcode, thinGBuffer );
            cmdList->bindImage( SSR::HiZTrace_HiZDepthBuffer_Hashcode, hizBuffer );
            cmdList->bindImage( SSR::HiZTrace_BlueNoise_Hashcode, blueNoise );
            cmdList->prepareAndBindResourceList();

            u32 ThreadGroupX = Max( 1u, SSR::HiZTraceProperties.OutputSize.x / SSR::HiZTrace_DispatchX );
            u32 ThreadGroupY = Max( 1u, SSR::HiZTraceProperties.OutputSize.y / SSR::HiZTrace_DispatchY );
            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, SSR::HiZTrace_DispatchZ );

			cmdList->popEventMarker();
		} 
	);
}

void SSRModule::destroy( RenderDevice& renderDevice )
{

}

void SSRModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
	blueNoise = graphicsAssetCache.getImage( DUSK_STRING( "GameData/textures/bluenoise.dds" ) );
}
