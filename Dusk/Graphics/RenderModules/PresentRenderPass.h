/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;
struct FGHandle;

void AddPresentRenderPass( FrameGraph& frameGraph, FGHandle imageToPresent );
void AddSwapchainStateTransition( FrameGraph& frameGraph );
