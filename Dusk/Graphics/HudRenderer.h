/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Rendering/CommandList.h>
#include <Maths/Matrix.h>

#include "Material.h"

class RenderDevice;
class ShaderCache;
class GraphicsAssetCache;
class BaseAllocator;
class FrameGraph;
class TextRenderingModule;
class LineRenderingModule;
class VirtualFileSystem;

struct CameraData;
struct Buffer;
struct Image;

using ResHandle_t = u32;

class HUDRenderer
{
public:
                            HUDRenderer( BaseAllocator* allocator );
                            HUDRenderer( HUDRenderer& ) = default;
                            ~HUDRenderer();

    // Destroy resources allocated by this renderer.
    void                    destroy( RenderDevice& renderDevice );

    // Load and create resources requested by this renderer.
    void                    loadCachedResources( RenderDevice& renderDevice, ShaderCache& shaderCache, GraphicsAssetCache& graphicsAssetCache );

    // Draw text (with outline) at given screen space coordinates.
    void                    drawText( const char* text, f32 size, f32 x, f32 y, const dkVec4f& textColor, const f32 outlineThickness );

    // Draw line between two given points (in screen space coordinates). If p0 color is different from p1 color,
    // an interpolation between the two colors will be applied.
    void                    drawLine( const dkVec3f& p0, const dkVec4f& p0Color, const dkVec3f& p1, const dkVec4f& p1Color, const f32 lineThickness );

    // Append render passes required to build the default HUD pipeline.
    // TODO In the future, this pipeline should be independent from the WorldRenderer pipeline (this way we'll be able to render
    // the HUD at the same time; and we'll simply have to blit both textures).
    ResHandle_t             buildDefaultGraph( FrameGraph& frameGraph, ResHandle_t presentRenderTarget );

private:
    // The memory allocator owning this instance.
    BaseAllocator*          memoryAllocator;

    // Module responsible for text rendering.
    TextRenderingModule*    textRendering;

    // Module responsible for line rendering (in screen space).
    LineRenderingModule*    lineRendering;
};
