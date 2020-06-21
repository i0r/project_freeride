/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;

#include <Graphics/RenderModules/FFTRenderPass.h> // Required by FFTPassOutput

class GlareRenderModule 
{
public:
                                GlareRenderModule();
                                GlareRenderModule( GlareRenderModule& ) = delete;
                                GlareRenderModule& operator = ( GlareRenderModule& ) = delete;
                                ~GlareRenderModule();

    // Convolute an input render target with the glare pattern of this module. The input render target need
    // to be in frequency domain.
    FFTPassOutput               addGlareComputePass( FrameGraph& frameGraph, FFTPassOutput fftInput );

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
    
    // Transform the active glare pattern into frequency domain.    
    void                        precomputePipelineResources( FrameGraph& frameGraph );

private:
    // The glare pattern used for convolution.
    Image*                      glarePatternTexture;

    // Glare pattern texture converted to frequency domain (0 is the real part; 1 is the imaginary part).
    Image*                      glarePatternFFT[2];
};
