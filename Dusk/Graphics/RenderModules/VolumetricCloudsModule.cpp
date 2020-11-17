/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "VolumetricCloudsModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

VolumetricCloudsModule::VolumetricCloudsModule()
    : noiseComputeModule()
{

}

VolumetricCloudsModule::~VolumetricCloudsModule()
{

}

void VolumetricCloudsModule::destroy( RenderDevice& renderDevice )
{
    noiseComputeModule.destroy( renderDevice );
}

void VolumetricCloudsModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    noiseComputeModule.loadCachedResources( renderDevice, graphicsAssetCache );
}

void VolumetricCloudsModule::precomputePipelineResources( FrameGraph& frameGraph )
{
    noiseComputeModule.precomputePipelineResources( frameGraph );
}
