/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/Types.h>
#include <Graphics/Mesh.h>

class RenderDevice;

struct Buffer;

class PrimitiveCache
{
public:
    struct Entry {
        Buffer* vertexBuffers[eMeshAttribute::Count];
        Buffer* indiceBuffer;
        u32     vertexBufferOffset;
        u32     indiceBufferOffset;
        u32     indiceCount;
    };

public:
    DUSK_INLINE const Entry& getSphereEntry() const { return sphereEntry; }

public:
            PrimitiveCache();
            PrimitiveCache( PrimitiveCache& ) = default;
            ~PrimitiveCache();

    void    destroy( RenderDevice* renderDevice );
    void    createCachedGeometry( RenderDevice* renderDevice );

private:
    Buffer* vertexAttributesBuffer[3];
    Buffer* indiceBuffer;

    Entry   sphereEntry;
};
