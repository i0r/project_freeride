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
class FrameGraphDebugWidget;
class CpuProfilerWidget;

class EditorInterface
{
public:
    DUSK_INLINE bool isViewportBeingResized() const { return isResizing; }

public:
            EditorInterface( BaseAllocator* allocator );
            ~EditorInterface();

    void    display( FrameGraph& frameGraph, ImGuiRenderModule* renderModule );

    void    loadCachedResources( GraphicsAssetCache* graphicsAssetCache );

private:
    // The memory allocator owning this module.
    BaseAllocator*  memoryAllocator;

#if DUSK_USE_RENDERDOC
    // Widget for RenderDoc capture.
    RenderDocHelperWidget* renderDocWidget;
#endif

    // Widget for FrameGraph debugging.
    FrameGraphDebugWidget* frameGraphWidget;

    // Widget for CPU Profiling.
    CpuProfilerWidget* cpuProfilerWidget;

    // Height of the main menubar (0 if the bar is disabled).
    f32             menuBarHeight;

    // True if the user if a resize operation is in progress (done by the user).
    bool            isResizing;

private:
	void    displayMenuBar();

	void    displayWindowMenu();

	void    displayGraphicsMenu();

	void    displayDebugMenu();

	void    displayEditMenu();

    void    displayFileMenu();

    void    placeNewEntityInWorld( const dkVec2f& viewportWinSize );
};
