/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

using ResHandle_t = uint32_t;

void AddPresentRenderPass( FrameGraph& frameGraph, ResHandle_t imageToPresent );
