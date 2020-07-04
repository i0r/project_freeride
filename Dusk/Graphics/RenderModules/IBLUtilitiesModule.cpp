/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "IBLUtilitiesModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/BRDFLut.generated.h"
#include <Graphics/RenderPasses/Headers/Light.h>

IBLUtilitiesModule::IBLUtilitiesModule()
    : brdfDfgLUT( nullptr )
{

}

IBLUtilitiesModule::~IBLUtilitiesModule()
{
    brdfDfgLUT = nullptr;
}

void IBLUtilitiesModule::destroy( RenderDevice& renderDevice )
{
    if ( brdfDfgLUT != nullptr ) {
        renderDevice.destroyImage( brdfDfgLUT );
        brdfDfgLUT = nullptr;
    }
}

void IBLUtilitiesModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    ImageDesc brdfLUTDesc;
    brdfLUTDesc.dimension = ImageDesc::DIMENSION_2D;
    brdfLUTDesc.format = eViewFormat::VIEW_FORMAT_R16G16_FLOAT;
    brdfLUTDesc.width = BRDF_LUT_SIZE;
    brdfLUTDesc.height = BRDF_LUT_SIZE;
    brdfLUTDesc.bindFlags = eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;
    brdfLUTDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;

    brdfDfgLUT = renderDevice.createImage( brdfLUTDesc );

#ifdef DUSK_DEVBUILD
    renderDevice.setDebugMarker( *brdfDfgLUT, DUSK_STRING( "BRDF LUT (Runtime Computed)" ) );
#endif
}

void IBLUtilitiesModule::precomputePipelineResources( FrameGraph& frameGraph )
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
            cmdList->bindImage( BrdfLut::ComputeBRDFLut_ComputedLUT_Hashcode, brdfDfgLUT );
            cmdList->prepareAndBindResourceList();

            u32 dispatchX = BRDF_LUT_SIZE / BrdfLut::ComputeBRDFLut_DispatchX;
            u32 dispatchY = BRDF_LUT_SIZE / BrdfLut::ComputeBRDFLut_DispatchY;
            cmdList->dispatchCompute( dispatchX, dispatchY, 1 );

            cmdList->popEventMarker();
        }
    );
}
