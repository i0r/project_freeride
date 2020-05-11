/*
Dusk Source Code
Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

using ResHandle_t = u32;

//TEST
void LoadCachedResourcesBP( RenderDevice& renderDevice, ShaderCache& shaderCache );
void FreeCachedResourcesBP( RenderDevice& renderDevice );

ResHandle_t AddBlurPyramidRenderPass( FrameGraph& frameGraph, ResHandle_t input, u32 inputWidth, u32 inputHeight );
