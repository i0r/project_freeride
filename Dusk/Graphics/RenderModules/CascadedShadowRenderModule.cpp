/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "CascadedShadowRenderModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Graphics/Mesh.h>
#include <Graphics/Model.h>
#include <Graphics/Material.h>

#include <Core/ViewFormat.h>

#include "Generated/DepthPyramid.generated.h"

DUSK_ENV_VAR( ShadowMapResolution, 2048, u32 ); // Per-slice shadow map resolution.

CascadedShadowRenderModule::CascadedShadowRenderModule()
{

}

CascadedShadowRenderModule::~CascadedShadowRenderModule()
{

}

void CascadedShadowRenderModule::destroy( RenderDevice& renderDevice )
{

}

void CascadedShadowRenderModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    
}

u32 DispatchSize( u32 tgSize, u32 numElements )
{
	u32 dispatchSize = numElements / tgSize;
    dispatchSize += numElements % tgSize > 0 ? 1 : 0;
    return dispatchSize;
}

void CascadedShadowRenderModule::captureShadowMap( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize )
{
    
	struct PassData {
		ResHandle_t	PerPassBuffer;
		ResHandle_t	DepthChain[16];
		i32			DepthLevelCount;
	};

	PassData& passData = frameGraph.addRenderPass<PassData>(
		DepthPyramid::DepthReduction_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			ImageDesc levelDesc;
			levelDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
			levelDesc.usage = RESOURCE_USAGE_DEFAULT;
			levelDesc.dimension = ImageDesc::DIMENSION_2D;
			levelDesc.format = eViewFormat::VIEW_FORMAT_R16G16_UNORM;
			levelDesc.width = static_cast< u32 >( depthBufferSize.x );
			levelDesc.height = static_cast< u32 >( depthBufferSize.y );

			passData.DepthLevelCount = 0;
            while ( levelDesc.width > 1 || levelDesc.height > 1 ) {
				levelDesc.width = DispatchSize( DepthPyramid::DepthReduction_DispatchX, levelDesc.width );
				levelDesc.height = DispatchSize( DepthPyramid::DepthReduction_DispatchY, levelDesc.height );

				passData.DepthChain[passData.DepthLevelCount++] = builder.allocateImage( levelDesc );
            }

            BufferDesc perPassBufferDesc;
            perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBufferDesc.SizeInBytes = sizeof( DepthPyramid::DepthReductionRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, DepthPyramid::DepthReductionMip0_ShaderBinding );

			cmdList->pushEventMarker( DepthPyramid::DepthReductionMip0_EventName );
			cmdList->bindPipelineState( pipelineState );

			Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
			
			// Bind resources
			cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

			cmdList->prepareAndBindResourceList();

			const CameraData* cameraData = resources->getMainCamera();

			DepthPyramid::DepthReductionProperties.CameraPlanes = dkVec2f( cameraData->nearPlane, cameraData->farPlane );
			DepthPyramid::DepthReductionProperties.Projection = cameraData->projectionMatrix;

			u32 width = static_cast< u32 >( depthBufferSize.x );
			u32 height = static_cast< u32 >( depthBufferSize.y );
            for ( i32 i = 0; i < passData.DepthLevelCount; i++ ) {
                DepthPyramid::DepthReductionProperties.TextureSize = dkVec2u( width, height );
                cmdList->updateBuffer( *passBuffer, &DepthPyramid::DepthReductionProperties, sizeof( DepthPyramid::DepthReductionRuntimeProperties ) );

                cmdList->dispatchCompute( width, height, DepthPyramid::DepthReduction_DispatchZ );
			}

			cmdList->popEventMarker();
		}
	);
}
