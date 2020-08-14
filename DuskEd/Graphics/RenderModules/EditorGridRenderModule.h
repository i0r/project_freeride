/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

#include "Graphics/FrameGraph.h"

class EditorGridModule
{
public:
								EditorGridModule();
								EditorGridModule( EditorGridModule& ) = delete;
								EditorGridModule& operator = ( EditorGridModule& ) = delete;
								~EditorGridModule();

	// Execute the frame composition pass (apply tonemapping; color correction; film grain; etc.).
	FGHandle                 addEditorGridPass( FrameGraph& frameGraph, FGHandle outputRenderTarget, FGHandle depthBuffer );
};
