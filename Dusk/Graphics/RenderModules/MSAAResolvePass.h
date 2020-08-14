/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

#include "Graphics/FrameGraph.h"

struct ResolvedPassOutput {
    FGHandle ResolvedColor;
    FGHandle ResolvedDepth;
};

FGHandle AddMSAAResolveRenderPass(
    FrameGraph& frameGraph,
    FGHandle inputImage,
    FGHandle inputVelocityImage,
    FGHandle inputDepthImage,
    const u32   sampleCount = 1,
    const bool  enableTAA = false
);

FGHandle AddMSAADepthResolveRenderPass(
    FrameGraph& frameGraph,
    FGHandle inputDepthImage,
    const u32   sampleCount = 1
);

FGHandle AddSSAAResolveRenderPass( 
    FrameGraph& frameGraph, 
    FGHandle resolvedInput, 
    bool isDepth = false
);
