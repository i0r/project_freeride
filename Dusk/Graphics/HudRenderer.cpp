/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "HUDRenderer.h"

#include "RenderModules/TextRenderingModule.h"
#include "RenderModules/LineRenderingModule.h"

HUDRenderer::HUDRenderer( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , textRendering( dk::core::allocate<TextRenderingModule>( allocator ) )
    , lineRendering( dk::core::allocate<LineRenderingModule>( allocator, allocator ) )
{

}

HUDRenderer::~HUDRenderer()
{
    dk::core::free( memoryAllocator, textRendering );
    dk::core::free( memoryAllocator, lineRendering );
}

void HUDRenderer::destroy( RenderDevice& renderDevice )
{
    textRendering->destroy( renderDevice );
    lineRendering->destroy( renderDevice );
}

void HUDRenderer::loadCachedResources( RenderDevice& renderDevice, ShaderCache& shaderCache, GraphicsAssetCache& graphicsAssetCache )
{
    textRendering->loadCachedResources( renderDevice, graphicsAssetCache );
    lineRendering->createPersistentResources( renderDevice );
}

void HUDRenderer::drawText( const char* text, f32 size, f32 x, f32 y, const dkVec4f& textColor, const f32 outlineThickness )
{
    // TODO In the future, we should defer the submission of texts later so that we don't
    // keep locking/unlocking the vertex buffer atomic...
    textRendering->addOutlinedText( text, size, x, y, textColor, outlineThickness );
}

void HUDRenderer::drawLine( const dkVec3f& p0, const dkVec4f& p0Color, const dkVec3f& p1, const dkVec4f& p1Color, const f32 lineThickness )
{
    lineRendering->addLine( p0, p0Color, p1, p1Color, lineThickness );
}

ResHandle_t HUDRenderer::buildDefaultGraph( FrameGraph& frameGraph, ResHandle_t presentRenderTarget )
{
    // Line Rendering.
    ResHandle_t hudRenderTarget = lineRendering->renderLines( frameGraph, presentRenderTarget );

    // HUD Text.
    hudRenderTarget = textRendering->renderText( frameGraph, hudRenderTarget );

    return hudRenderTarget;
}
