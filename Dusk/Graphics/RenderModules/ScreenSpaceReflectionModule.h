/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;

class SSRModule 
{
public:
                                SSRModule();
                                SSRModule( SSRModule& ) = delete;
                                SSRModule& operator = ( SSRModule& ) = delete;
                                ~SSRModule();

    void                        computeHiZMips( FrameGraph& frameGraph, ResHandle_t resolvedDepthBuffer, const u32 width, const u32 height );

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
    
private:
};
