/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Maths/Vector.h>

struct Buffer;

class FrameGraph;
class RenderDevice;
class BaseAllocator;

using ResHandle_t = uint32_t;

class LineRenderingModule
{
public:
                    LineRenderingModule( BaseAllocator* allocator );
                    LineRenderingModule( LineRenderingModule& ) = delete;
                    ~LineRenderingModule();

    void            destroy( RenderDevice& renderDevice );
    ResHandle_t     renderLines( FrameGraph& frameGraph, ResHandle_t output );

    void            addLine( const dkVec3f& p0, const dkVec4f& p0Color, const dkVec3f& p1, const dkVec4f& p1Color, const f32 lineThickness );
    void            createPersistentResources( RenderDevice& renderDevice );

private:
    static constexpr i32 BUFFER_COUNT = 2;

private:
    // The memory allocator owning this instance.
    BaseAllocator*  memoryAllocator;

    // Indice count to draw next time the rendering function is called.
    i32             indiceCount;

    // The index of the vertex buffer being used to render the lines.
    i32             vertexBufferWriteIndex;

    // Data index in the float buffer.
    i32             bufferIndex;

    // Vertex buffers used by the module. We use a "ping-pong" buffer swapping; lineVertexBuffers[vertexBufferWriteIndex]
    // is the buffer used for rendering.
    Buffer*         lineVertexBuffers[BUFFER_COUNT];

    // Indice buffer used by the module. We precompute its content at initialization time.
    Buffer*         lineIndiceBuffer;

    // Vertex buffer content (uploaded once every frame).
    f32*            buffer;

    std::atomic<bool>   vertexBufferLock;

    std::atomic<bool>   vertexBufferRenderingLock;

private:
    void            lockVertexBuffer();
    void            unlockVertexBuffer();

    void            lockVertexBufferRendering();
    void            unlockVertexBufferRendering();

};