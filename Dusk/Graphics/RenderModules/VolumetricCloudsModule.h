/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;

#include "Graphics/FrameGraph.h"
#include "VolumetricCloudsNoiseCompute.h"

class VolumetricCloudsModule 
{
public:
                                    VolumetricCloudsModule();
                                    VolumetricCloudsModule( VolumetricCloudsModule& ) = delete;
                                    VolumetricCloudsModule& operator = ( VolumetricCloudsModule& ) = delete;
                                    ~VolumetricCloudsModule();

    // Release and destroy persistent resources created by this module.
    void                            destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                            loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
 
    void                            precomputePipelineResources( FrameGraph& frameGraph );

private:
    VolumetricCloudsNoiseCompute    noiseComputeModule;
};
