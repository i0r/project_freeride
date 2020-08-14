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

class FrameCompositionModule 
{
public:
                                FrameCompositionModule();
                                FrameCompositionModule( FrameCompositionModule& ) = delete;
                                FrameCompositionModule& operator = ( FrameCompositionModule& ) = delete;
                                ~FrameCompositionModule();

    // Execute the frame composition pass (apply tonemapping; color correction; film grain; etc.).
    FGHandle                 addFrameCompositionPass( FrameGraph& frameGraph, FGHandle input, FGHandle glareInput );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( GraphicsAssetCache& graphicsAssetCache );
    
private:
    // The LUT used to perform color correction.
    Image*                      colorGradingLUT;
};
