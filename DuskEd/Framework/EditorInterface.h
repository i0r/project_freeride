/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class ImGuiRenderModule;
class RenderDocHelperWidget;
class BaseAllocator;
class GraphicsAssetCache;
class FrameGraph;

class EditorInterface
{
public:
            EditorInterface( BaseAllocator* allocator );
            ~EditorInterface();

    void    display( FrameGraph& frameGraph, ImGuiRenderModule* renderModule );

    void    loadCachedResources( GraphicsAssetCache* graphicsAssetCache );

private:
    // Height of the main menubar (0 if the bar is disabled).
    f32     menuBarHeight;

    // The memory allocator owning this module.
    BaseAllocator* memoryAllocator;

#if DUSK_USE_RENDERDOC
    // Widget for RenderDoc capture.
	RenderDocHelperWidget* renderDocWidget;
#endif

private:
	void    displayMenuBar();

	void    displayWindowMenu();

	void    displayGraphicsMenu();

	void    displayDebugMenu();

	void    displayEditMenu();

    void    displayFileMenu();
};
