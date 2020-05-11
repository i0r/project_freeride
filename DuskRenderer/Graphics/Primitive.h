/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Core/Types.h>

// The most basic entity to submit geometry to the renderer. It can be used to render any higher level entity (e.g. 
// a skinned model, a static mesh, etc.).
struct Primitive 
{
    // An array of buffer used to store this primitive attributes. Note that you might use an interleaved buffer holding
    // multiple attributes at once.
    Buffer**    AttributeBuffers;

    // AttributeBuffers number of buffers.
    u32         AttributeBufferCount;

    // The index buffer (if null, means the primitive is not indexed and won't use indexed draw).
    Buffer*     IndexBuffer;
};
