/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "AutomaticExposure.h"

#include <Graphics/FrameGraph.h>
#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Framework/Cameras/Camera.h>

#include "Generated/AutoExposure.generated.h"

constexpr i32 EXPOSURE_INFO_BUFFER_COUNT = 2;

AutomaticExposureModule::AutomaticExposureModule()
    : autoExposureBuffer{ nullptr, nullptr }
    , exposureTarget( 0 )
    , autoExposureInfo()
{

}

AutomaticExposureModule::~AutomaticExposureModule()
{

}

void AutomaticExposureModule::importResourcesToGraph( FrameGraph& frameGraph ) 
{
    frameGraph.importPersistentBuffer( PERSISTENT_BUFFER_HASHCODE_READ, autoExposureBuffer[exposureTarget] );
    frameGraph.importPersistentBuffer( PERSISTENT_BUFFER_HASHCODE_WRITE, autoExposureBuffer[( exposureTarget == 0 ) ? 1 : 0] );

    // Swap buffers
    exposureTarget = ( ++exposureTarget % EXPOSURE_INFO_BUFFER_COUNT );
}

FGHandle AutomaticExposureModule::computeExposure( FrameGraph& frameGraph, FGHandle lightRenderTarget, const dkVec2u& screenSize )
{
    // Update persistent buffer pointers
    FGHandle histoBins = addBinComputePass( frameGraph, lightRenderTarget, screenSize );
    FGHandle mergedBuffer = addHistogramMergePass( frameGraph, histoBins, screenSize );
    FGHandle autoExposureInfo = addExposureComputePass( frameGraph, mergedBuffer );

    return autoExposureInfo;
}

void AutomaticExposureModule::destroy( RenderDevice& renderDevice )
{
    for ( i32 i = 0; i < EXPOSURE_INFO_BUFFER_COUNT; i++ ) {
        renderDevice.destroyBuffer( autoExposureBuffer[i] );
        autoExposureBuffer[i] = nullptr;
    }
}

void AutomaticExposureModule::loadCachedResources( RenderDevice& renderDevice )
{
    // Create Ping-Pong Auto Exposure Buffers
    BufferDesc autoExposureBufferDescription;
    autoExposureBufferDescription.BindFlags = RESOURCE_BIND_STRUCTURED_BUFFER | RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
    autoExposureBufferDescription.Usage = RESOURCE_USAGE_DEFAULT;
    autoExposureBufferDescription.SizeInBytes = sizeof( AutoExposureInfo );
    autoExposureBufferDescription.StrideInBytes = sizeof( AutoExposureInfo );
    autoExposureBufferDescription.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

    for ( i32 i = 0; i < EXPOSURE_INFO_BUFFER_COUNT; i++ ) {
        autoExposureBuffer[i] = renderDevice.createBuffer( autoExposureBufferDescription, &autoExposureInfo );
    }
}

FGHandle AutomaticExposureModule::addBinComputePass( FrameGraph& frameGraph, const FGHandle inputRenderTarget, const dkVec2u& screenSize )
{
    struct PassData {
        FGHandle Input;
        FGHandle Output;
        FGHandle PerViewBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        AutoExposure::BinCompute_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.Input = builder.readImage( inputRenderTarget );

            // Per Tile Histogram
            const uint32_t bufferDimension = ( static_cast<uint32_t>( screenSize.y + 3 ) >> 2 ) * 128;
            const size_t perTileHistogramBufferSize = sizeof( u32 ) * static_cast<std::size_t>( bufferDimension );

            // Create Per-Tile Histogram Buffer
            BufferDesc perTileHistogramsBufferDescription;
            perTileHistogramsBufferDescription.Usage = RESOURCE_USAGE_DEFAULT;
            perTileHistogramsBufferDescription.BindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            perTileHistogramsBufferDescription.SizeInBytes = static_cast< u32 >( perTileHistogramBufferSize );
            perTileHistogramsBufferDescription.StrideInBytes = bufferDimension;
            perTileHistogramsBufferDescription.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

            passData.Output = builder.allocateBuffer( perTileHistogramsBufferDescription, SHADER_STAGE_COMPUTE );
            passData.PerViewBuffer = builder.retrievePerViewBuffer();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* inputTarget = resources->getImage( passData.Input );
            Buffer* outputBuffer = resources->getBuffer( passData.Output );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );
            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, AutoExposure::BinCompute_ShaderBinding );
            
            cmdList->pushEventMarker( AutoExposure::BinCompute_EventName );
            cmdList->bindPipelineState( pipelineState );

            cmdList->bindImage( AutoExposure::BinCompute_InputRenderTarget_Hashcode, inputTarget );
            cmdList->bindBuffer( AutoExposure::BinCompute_HistogramBuffer_Hashcode, outputBuffer, VIEW_FORMAT_R32_UINT );
            cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1u, static_cast< u32 >( ceilf( screenSize.y / static_cast<f32>( AutoExposure::BinCompute_DispatchY ) ) ), 1u );

            cmdList->popEventMarker();
        }
    );

    return passData.Output;
}

FGHandle AutomaticExposureModule::addHistogramMergePass( FrameGraph& frameGraph, const FGHandle perTileHistoBuffer, const dkVec2u& screenSize )
{
    struct PassData {
        FGHandle Input;
        FGHandle Output;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        AutoExposure::HistogramMerge_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.Input = builder.readBuffer( perTileHistoBuffer );

            const u32 backbufferHistogramSize = sizeof( u32 ) * 128;

            // Create Final Histogram Buffer
            BufferDesc backbufferHistogramsBufferDescription;
            backbufferHistogramsBufferDescription.BindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            backbufferHistogramsBufferDescription.Usage = RESOURCE_USAGE_DEFAULT;
            backbufferHistogramsBufferDescription.SizeInBytes = backbufferHistogramSize;
            backbufferHistogramsBufferDescription.StrideInBytes = 128;
            backbufferHistogramsBufferDescription.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

            passData.Output = builder.allocateBuffer( backbufferHistogramsBufferDescription, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* inputBuffer = resources->getBuffer( passData.Input );
            Buffer* outputBuffer = resources->getBuffer( passData.Output );
            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, AutoExposure::HistogramMerge_ShaderBinding );

            cmdList->pushEventMarker( AutoExposure::HistogramMerge_EventName );
            cmdList->bindPipelineState( pipelineState );

            cmdList->bindBuffer( AutoExposure::HistogramMerge_Histogram_Hashcode, inputBuffer, VIEW_FORMAT_R32_UINT );
            cmdList->bindBuffer( AutoExposure::HistogramMerge_FinalHistogram_Hashcode, outputBuffer, VIEW_FORMAT_R32_UINT );

            cmdList->prepareAndBindResourceList();
            cmdList->dispatchCompute( 128u, 1u, 1u );

            cmdList->popEventMarker();
        }
    );

    return passData.Output;
}

FGHandle AutomaticExposureModule::addExposureComputePass( FrameGraph& frameGraph, const FGHandle mergedHistoBuffer )
{
    struct PassData {
        FGHandle Input;
        FGHandle Output;

        FGHandle LastFrameOutput;
        FGHandle ParametersBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        AutoExposure::TileHistogramCompute_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.Input = builder.readBuffer( mergedHistoBuffer );
            passData.Output = builder.retrievePersistentBuffer( PERSISTENT_BUFFER_HASHCODE_WRITE );
            passData.LastFrameOutput = builder.retrievePersistentBuffer( PERSISTENT_BUFFER_HASHCODE_READ );

            BufferDesc parametersBufferDesc;
            parametersBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            parametersBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            parametersBufferDesc.SizeInBytes = sizeof( AutoExposure::TileHistogramComputeRuntimeProperties );

            passData.ParametersBuffer = builder.allocateBuffer( parametersBufferDesc, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* inputBuffer = resources->getBuffer( passData.Input );
            Buffer* parametersBuffer = resources->getBuffer( passData.ParametersBuffer );

            AutoExposure::TileHistogramComputeProperties.DeltaTime = resources->getDeltaTime() * 0.01f;

            // Finally, compute auto exposure infos
            Buffer* bufferToRead = resources->getPersistentBuffer( passData.LastFrameOutput );
            Buffer* bufferToWrite = resources->getPersistentBuffer( passData.Output );
            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, AutoExposure::TileHistogramCompute_ShaderBinding );

            cmdList->pushEventMarker( AutoExposure::TileHistogramCompute_EventName );
            cmdList->bindPipelineState( pipelineState );

            cmdList->bindConstantBuffer( PerPassBufferHashcode, parametersBuffer );
            cmdList->bindBuffer( AutoExposure::TileHistogramCompute_Histogram_Hashcode, inputBuffer, VIEW_FORMAT_R32_UINT );
            cmdList->bindBuffer( AutoExposure::TileHistogramCompute_RAutoExposureBuffer_Hashcode, bufferToRead, VIEW_FORMAT_R32_UINT );
            cmdList->bindBuffer( AutoExposure::TileHistogramCompute_WAutoExposureBuffer_Hashcode, bufferToWrite, VIEW_FORMAT_R32_UINT );

            cmdList->prepareAndBindResourceList();

            cmdList->updateBuffer( *parametersBuffer, &AutoExposure::TileHistogramComputeProperties, sizeof( AutoExposure::TileHistogramComputeRuntimeProperties ) );
            
            cmdList->dispatchCompute( 1u, 1u, 1u );

            cmdList->popEventMarker();
        }
    );

    return passData.Output;
}
