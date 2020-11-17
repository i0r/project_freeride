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

class VolumetricCloudsNoiseCompute 
{
public:
                                VolumetricCloudsNoiseCompute();
                                VolumetricCloudsNoiseCompute( VolumetricCloudsNoiseCompute& ) = delete;
                                VolumetricCloudsNoiseCompute& operator = ( VolumetricCloudsNoiseCompute& ) = delete;
                                ~VolumetricCloudsNoiseCompute();

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
 
    void                        precomputePipelineResources( FrameGraph& frameGraph );

private:
    // Worley/Perlin Noise RenderTarget (used for clouds shape).
    Image*                      shapeNoiseTexture;
};
