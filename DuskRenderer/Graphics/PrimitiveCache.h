/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/Types.h>

class RenderDevice;
class ShaderCache;
class GraphicsAssetCache;

struct Buffer;

class PrimitiveCache
{
public:
    struct Entry {
        Buffer* vertexBuffers[3];
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
    void    loadCachedResources( RenderDevice* renderDevice, ShaderCache* shaderCache, GraphicsAssetCache* graphicsAssetCache );

private:
    Buffer* vertexAttributesBuffer[3];
    Buffer* indiceBuffer;

    Entry   sphereEntry;
};
