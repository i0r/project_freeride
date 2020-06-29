/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;

using ResHandle_t = u32;

class IBLUtilitiesModule 
{
public:
                                IBLUtilitiesModule();
                                IBLUtilitiesModule( IBLUtilitiesModule& ) = delete;
                                IBLUtilitiesModule& operator = ( IBLUtilitiesModule& ) = delete;
                                ~IBLUtilitiesModule();

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

    // Precompute BRDF DFG LUT.
    void                        precomputePipelineResources( FrameGraph& frameGraph );

private:
    Image*                      brdfDfgLUT;
};
