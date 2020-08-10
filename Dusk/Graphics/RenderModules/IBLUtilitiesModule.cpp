/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "IBLUtilitiesModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/BRDFLut.generated.h"
#include "Generated/IBL.generated.h"

#include <Graphics/ShaderHeaders/Light.h>

void AddCubeFaceIrradianceComputePass( FrameGraph& frameGraph, Image* cubemap, Image* irradianceCube, const u32 faceIndex )
{
	struct PassData {
		ResHandle_t         PerPassBuffer;
	};

	PassData& passData = frameGraph.addRenderPass<PassData>(
		IBL::ComputeIrradianceMap_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			BufferDesc perPassBufferDesc;
			perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
			perPassBufferDesc.SizeInBytes = sizeof( IBL::ComputeIrradianceMapRuntimeProperties );
			passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_TrilinearClampEdge );

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, IBL::ComputeIrradianceMap_ShaderBinding );

			// Update Parameters
            IBL::ComputeIrradianceMapProperties.CubeFace = faceIndex;

			cmdList->pushEventMarker( IBL::ComputeIrradianceMap_EventName );
			cmdList->bindPipelineState( pipelineState );

			Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
			
			ImageViewDesc cubeView;
			cubeView.ImageCount = 1;
			cubeView.MipCount = 1;
			cubeView.StartImageIndex = faceIndex;
			cubeView.StartMipIndex = 0;

			// Bind resources
            cmdList->bindImage( IBL::ComputeIrradianceMap_IrradianceMap_Hashcode, irradianceCube, cubeView );
            cmdList->bindImage( IBL::ComputeIrradianceMap_EnvironmentCube_Hashcode, cubemap );

			cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

			cmdList->prepareAndBindResourceList();

			cmdList->updateBuffer( *passBuffer, &IBL::ComputeIrradianceMapProperties, sizeof( IBL::ComputeIrradianceMapRuntimeProperties ) );

			constexpr u32 ThreadCountX = 64 / IBL::ComputeIrradianceMap_DispatchX;
			constexpr u32 ThreadCountY = 64 / IBL::ComputeIrradianceMap_DispatchY;

			cmdList->dispatchCompute( ThreadCountX, ThreadCountY, IBL::ComputeIrradianceMap_DispatchZ );

			cmdList->popEventMarker();
		}
	);
}

void AddCubeFaceFilteringPass( FrameGraph& frameGraph, Image* cubemap, Image* filteredCube, const u32 faceIndex, const u32 mipIndex )
{
	struct PassData {
		ResHandle_t	PerPassBuffer;
	};

	PassData& passData = frameGraph.addRenderPass<PassData>(
		IBL::FilterCubeFace_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			BufferDesc perPassBufferDesc;
			perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
			perPassBufferDesc.SizeInBytes = sizeof( IBL::FilterCubeFaceRuntimeProperties );
			passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_TrilinearClampEdge );

			// Update Parameters
			u32 currentFaceSize = ( PROBE_FACE_SIZE >> mipIndex );
			f32 roughness = mipIndex / Max( 1.0f, std::log2( currentFaceSize ) );
			const bool isRoughnessZero = ( roughness == 0.0f );

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, ( isRoughnessZero ) ? IBL::FilterCubeFace0_ShaderBinding : IBL::FilterCubeFace_ShaderBinding );

			IBL::FilterCubeFaceProperties.CubeFace = faceIndex;
			IBL::FilterCubeFaceProperties.WidthActiveMip = static_cast<f32>( currentFaceSize );
			IBL::FilterCubeFaceProperties.Roughness = roughness;

			cmdList->pushEventMarker( IBL::FilterCubeFace_EventName );
			cmdList->bindPipelineState( pipelineState );

			Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
			
			ImageViewDesc cubeView;
			cubeView.ImageCount = 1;
			cubeView.MipCount = 1;
			cubeView.StartImageIndex = faceIndex;
			cubeView.StartMipIndex = mipIndex;

			// Bind resources
            cmdList->bindImage( IBL::FilterCubeFace_FilteredCubeFace_Hashcode, filteredCube, cubeView );
            cmdList->bindImage( IBL::FilterCubeFace_EnvironmentCube_Hashcode, cubemap );

			cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

			cmdList->prepareAndBindResourceList();

			cmdList->updateBuffer( *passBuffer, &IBL::FilterCubeFaceProperties, sizeof( IBL::FilterCubeFaceRuntimeProperties ) );

			const u32 ThreadCountX = currentFaceSize / IBL::FilterCubeFace_DispatchX;
			const u32 ThreadCountY = currentFaceSize / IBL::FilterCubeFace_DispatchY;

			cmdList->dispatchCompute( ThreadCountX, ThreadCountY, IBL::FilterCubeFace_DispatchZ );

			cmdList->popEventMarker();
		}
	);
}

void AddBrdfDfgLutComputePass( FrameGraph& frameGraph, Image* brdfDfgLut )
{
    struct PassData {};

    frameGraph.addRenderPass<PassData>(
        BrdfLut::ComputeBRDFLut_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            cmdList->pushEventMarker( BrdfLut::ComputeBRDFLut_EventName );

            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, BrdfLut::ComputeBRDFLut_ShaderBinding );
            cmdList->bindPipelineState( pso );
            cmdList->bindImage( BrdfLut::ComputeBRDFLut_ComputedLUT_Hashcode, brdfDfgLut );
            cmdList->prepareAndBindResourceList();

            u32 dispatchX = BRDF_LUT_SIZE / BrdfLut::ComputeBRDFLut_DispatchX;
            u32 dispatchY = BRDF_LUT_SIZE / BrdfLut::ComputeBRDFLut_DispatchY;
            cmdList->dispatchCompute( dispatchX, dispatchY, 1 );

            cmdList->popEventMarker();
        }
    );
}
