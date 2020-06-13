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

class FrameCompositionModule 
{
public:
                                FrameCompositionModule();
                                FrameCompositionModule( FrameCompositionModule& ) = delete;
                                FrameCompositionModule& operator = ( FrameCompositionModule& ) = delete;
                                ~FrameCompositionModule();

    // Execute the frame composition pass (apply tonemapping; color correction; film grain; etc.).
    ResHandle_t                 addFrameCompositionPass( FrameGraph& frameGraph, ResHandle_t input, ResHandle_t glareInput );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( GraphicsAssetCache& graphicsAssetCache );
    
private:
    // The LUT used to perform color correction.
    Image*                      colorGradingLUT;
};
