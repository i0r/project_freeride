/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Graphics/FrameGraph.h"

FGHandle AddMSAAResolveRenderPass(
    FrameGraph& frameGraph,
    FGHandle    inputImage,
    FGHandle    inputVelocityImage,
    FGHandle    inputDepthImage,
    const u32   sampleCount = 1u,
    const bool  enableTAA = false
);

FGHandle AddCheapMSAAResolveRenderPass(
    FrameGraph& frameGraph,
    FGHandle    inputImage,
    const u32   sampleCount = 1u
);

FGHandle AddMSAADepthResolveRenderPass(
    FrameGraph& frameGraph,
    FGHandle    inputDepthImage,
    const u32   sampleCount = 1u
);

FGHandle AddSSAAResolveRenderPass( 
    FrameGraph& frameGraph, 
    FGHandle    resolvedInput, 
    bool        isDepth = false
);
