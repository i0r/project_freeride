/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

using ResHandle_t = uint32_t;

ResHandle_t AddFinalPostFxRenderPass( FrameGraph& frameGraph, ResHandle_t input, ResHandle_t glareInput );
