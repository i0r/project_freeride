/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "VolumetricCloudsNoiseCompute.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/VolumetricCloudsNoise.generated.h"
#include "Graphics/ShaderHeaders/Light.h"
#include "AutomaticExposure.h"

VolumetricCloudsNoiseCompute::VolumetricCloudsNoiseCompute()
    : shapeNoiseTexture( nullptr )
{

}

VolumetricCloudsNoiseCompute::~VolumetricCloudsNoiseCompute()
{
    shapeNoiseTexture = nullptr;
}

void VolumetricCloudsNoiseCompute::destroy( RenderDevice& renderDevice )
{
    if ( shapeNoiseTexture != nullptr ) {
        renderDevice.destroyImage( shapeNoiseTexture );
        shapeNoiseTexture = nullptr;
    }
}

void VolumetricCloudsNoiseCompute::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    ImageDesc imageDesc;
    imageDesc.dimension = ImageDesc::DIMENSION_3D;
    imageDesc.format = eViewFormat::VIEW_FORMAT_R8G8B8A8_UNORM;
    imageDesc.width = static_cast<u32>( ShapeNoiseTexSize.x );
    imageDesc.height = static_cast< u32 >( ShapeNoiseTexSize.y );
    imageDesc.depth = static_cast< u32 >( ShapeNoiseTexSize.z );
    imageDesc.mipCount = 6;
    imageDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;
    imageDesc.bindFlags = eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;

    shapeNoiseTexture = renderDevice.createImage( imageDesc );

#if DUSK_DEVBUILD
    renderDevice.setDebugMarker( *shapeNoiseTexture, DUSK_STRING( "VolumetricCloudsNoiseCompute/ShapeNoiseTexture" ) );
#endif
}

void VolumetricCloudsNoiseCompute::precomputePipelineResources( FrameGraph& frameGraph )
{
    struct PassData {};

    frameGraph.addRenderPass<PassData>(
        VolumetricCloudsNoise::ShapeNoise_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, VolumetricCloudsNoise::ShapeNoise_ShaderBinding );
            cmdList->pushEventMarker( VolumetricCloudsNoise::ShapeNoise_EventName );
            cmdList->bindPipelineState( pso );

            // Bind resources
            cmdList->bindImage( VolumetricCloudsNoise::ShapeNoise_ComputedNoiseTexture_Hashcode, shapeNoiseTexture );
            cmdList->prepareAndBindResourceList();

            const u32 ThreadGroupX = DispatchSize( VolumetricCloudsNoise::ShapeNoise_DispatchX, static_cast< u32 >( ShapeNoiseTexSize.x ) );
            const u32 ThreadGroupY = DispatchSize( VolumetricCloudsNoise::ShapeNoise_DispatchY, static_cast< u32 >( ShapeNoiseTexSize.y ) );
            const u32 ThreadGroupZ = DispatchSize( VolumetricCloudsNoise::ShapeNoise_DispatchZ, static_cast< u32 >( ShapeNoiseTexSize.z ) );

            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, ThreadGroupZ );

            cmdList->popEventMarker();
        }
    );
}
