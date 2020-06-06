/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Maths/Vector.h>
#include <Graphics/FrameGraph.h>

struct Image;
struct FontDescriptor;
struct PipelineState;
struct Buffer;

class FrameGraph;
class RenderDevice;
class GraphicsAssetCache;

namespace
{
    static constexpr i32 MaxCharactersPerLine = 240;
    static constexpr i32 MaxCharactersLines = 65;
    static constexpr i32 TEXT_RENDERING_MAX_GLYPH_COUNT = MaxCharactersPerLine * MaxCharactersLines * 4;
}

class TextRenderingModule
{
public:
                        TextRenderingModule();
                        TextRenderingModule( TextRenderingModule& ) = delete;
                        TextRenderingModule& operator = ( TextRenderingModule& ) = delete;
                        ~TextRenderingModule();
                        
    void                destroy( RenderDevice& renderDevice );
    ResHandle_t         renderText( FrameGraph& frameGraph, ResHandle_t output );

    void                loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
    void                addOutlinedText( const char* text, f32 size, f32 x, f32 y, const dkVec4f& textColor = dkVec4f( 0.8f, 0.8f, 0.8f, 1.0f ), const f32 outlineThickness = 0.80f );

private:
    static constexpr i32 BUFFER_COUNT = 2;

private:
    void                        lockVertexBuffer();
    void                        unlockVertexBuffer();

    void                        lockVertexBufferRendering();
    void                        unlockVertexBufferRendering();

private:
    Image*                      fontAtlas;
    FontDescriptor*             fontDescriptor;

    u32                         vertexBufferIndex;
    Buffer*                     glyphVertexBuffers[BUFFER_COUNT];

    Buffer*                     glyphIndiceBuffer;

    f32                         buffer[TEXT_RENDERING_MAX_GLYPH_COUNT];

    std::atomic<i32>            bufferOffset;
    std::atomic<u32>            indiceCount;
    std::atomic<bool>           vertexBufferLock;

    std::atomic<bool>           vertexBufferRenderingLock;
};
