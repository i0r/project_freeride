/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

using ResHandle_t = uint32_t;

class EditorGridModule
{
public:
								EditorGridModule();
								EditorGridModule( EditorGridModule& ) = delete;
								EditorGridModule& operator = ( EditorGridModule& ) = delete;
								~EditorGridModule();

	// Execute the frame composition pass (apply tonemapping; color correction; film grain; etc.).
	ResHandle_t                 addEditorGridPass( FrameGraph& frameGraph, ResHandle_t outputRenderTarget, ResHandle_t depthBuffer );
};
