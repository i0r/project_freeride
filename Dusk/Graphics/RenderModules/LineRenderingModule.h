/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Maths/Vector.h>
#include <Graphics/FrameGraph.h>

struct Buffer;
struct LineInfos;

class FrameGraph;
class RenderDevice;
class BaseAllocator;

class LineRenderingModule
{
public:
                        LineRenderingModule( BaseAllocator* allocator );
                        LineRenderingModule( LineRenderingModule& ) = delete;
                        ~LineRenderingModule();

    void                destroy( RenderDevice& renderDevice );
    FGHandle         renderLines( FrameGraph& frameGraph, FGHandle output );

    void                addLine( const dkVec3f& p0, const dkVec4f& p0Color, const dkVec3f& p1, const dkVec4f& p1Color, const f32 lineThickness );
    void                createPersistentResources( RenderDevice& renderDevice );

private:
    // The memory allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // A constant buffer holding informations to render lines.
    Buffer*             linePointsConstantBuffer;

    // Constant buffer content (uploaded once every frame).
    LineInfos*          linePointsToRender;

    // Line count to render next time this module is rendered.
    i32                 lineCount;

    // Atomic lock for cbuffer upload. When the lock is true, the access from a rendering worker is blocked until the
    // atomic is unlocked.
    std::atomic<bool>   cbufferLock;

    // Atomic lock for line rendering. When the lock is true, the access from the logic thread is blocked until the
    // atomic is unlocked.
    std::atomic<bool>   cbufferRenderingLock;

private:
    void                lockVertexBuffer();
    void                unlockVertexBuffer();

    void                lockVertexBufferRendering();
    void                unlockVertexBufferRendering();

};