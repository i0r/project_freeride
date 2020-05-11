/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

using ResHandle_t = uint32_t;

ResHandle_t AddMSAAResolveRenderPass( 
    FrameGraph& frameGraph,
    ResHandle_t inputImage, 
    ResHandle_t inputVelocityImage, 
    ResHandle_t inputDepthImage, 
    const u32   sampleCount = 1, 
    const bool  enableTAA = false 
);
