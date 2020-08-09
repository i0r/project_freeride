/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "ScreenSpaceReflectionModule.h"

#include <Graphics/FrameGraph.h>
#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>

#include "Graphics/RenderPasses/Headers/Light.h"

#include "Graphics/RenderModules/Generated/DepthPyramid.generated.h"

SSRModule::SSRModule()
{

}

SSRModule::~SSRModule()
{

}

void SSRModule::computeHiZMips( FrameGraph& frameGraph, ResHandle_t resolvedDepthBuffer, const u32 width, const u32 height )
{
	struct PassData
	{
		ResHandle_t PerPassBuffer;
		ResHandle_t	DepthBufferMip0;
		ResHandle_t DepthMips[SSR_MAX_MIP_LEVEL];
		i32         HiZMipCount;
		u32			Mip0Width;
		u32			Mip0Height;
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
			passData.HiZMipCount = 0;
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

			cmdList->popEventMarker();
		} 
	);
}

void SSRModule::destroy( RenderDevice& renderDevice )
{

}

void SSRModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{

}
