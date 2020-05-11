/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;
struct Buffer;

using ResHandle_t = uint32_t;

// TEST STUFF (TO BE REMOVED ONCE THE SCENE/STREAMING IS IMPLEMENTED)
struct BuiltPrimitive {
    // Position / Normal / UvCoords
    const Buffer* VertexAttributeBuffer[3];
    Buffer* IndiceBuffer;
    u32     IndiceCount;
    u32     VertexCount;
};

#include "PrimitiveLighting.generated.h"

PrimitiveLighting::PrimitiveLighting_Generic_Output AddPrimitiveLightTest( FrameGraph& frameGraph, BuiltPrimitive& primitive, ResHandle_t perSceneBuffer );
