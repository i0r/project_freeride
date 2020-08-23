/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;
struct FGHandle;

class SSRModule 
{
public:
    struct HiZResult {
        // TextureArray holding each mip level of the HiZ buffer (required for older APIs).
        // This is unused for current gen APIs (i.e. Vulkan/D3D12) since it introduces an implicit resource copy.
        FGHandle    HiZMerged;

        // Array of Texture holding each mip level (used for current gen APIs).
        FGHandle	HiZMips[SSR_MAX_MIP_LEVEL];

        HiZResult()
            : HiZMerged( FGHandle::Invalid )
        {

        }
    };

    struct TraceResult {
        FGHandle TraceBuffer;

        FGHandle MaskBuffer;

        TraceResult()
            : TraceBuffer( FGHandle::Invalid )
            , MaskBuffer( FGHandle::Invalid )
        {

        }
    };

public:
                                SSRModule();
                                SSRModule( SSRModule& ) = delete;
                                SSRModule& operator = ( SSRModule& ) = delete;
                                ~SSRModule();

    HiZResult                   computeHiZMips( FrameGraph& frameGraph, FGHandle resolvedDepthBuffer, const u32 width, const u32 height );
    
    TraceResult                 rayTraceHiZ( FrameGraph& frameGraph, FGHandle resolvedDepthBuffer, FGHandle resolveThinGbuffer, const HiZResult& hiZBuffer, const u32 width, const u32 height );

    FGHandle                    resolveRaytrace( FrameGraph& frameGraph, const TraceResult& traceResult, FGHandle depthBuffer, FGHandle colorBuffer, FGHandle resolvedThinGbuffer, const u32 width, const u32 height );

    FGHandle                    temporalRebuild( FrameGraph& frameGraph, FGHandle rayTraceOutput, FGHandle resolvedOutput, const u32 width, const u32 height );

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the hard drive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

private:
    // Offline-generated blue noise texture for stochastic ray tracing.
    Image*                      blueNoise;
};
