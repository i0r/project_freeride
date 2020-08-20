/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Material;

#include "Mesh.h"

struct DrawCommandKey
{
    enum Layer : uint8_t
    {
        LAYER_DEPTH,
        LAYER_WORLD,
        LAYER_HUD,
        LAYER_DEBUG
    };

    enum DepthViewportLayer : uint8_t
    {
        DEPTH_VIEWPORT_LAYER_DEFAULT,
        DEPTH_VIEWPORT_LAYER_CSM0,
        DEPTH_VIEWPORT_LAYER_CSM1,
        DEPTH_VIEWPORT_LAYER_CSM2,
        DEPTH_VIEWPORT_LAYER_CSM3
    };

    enum WorldViewportLayer : uint8_t
    {
        WORLD_VIEWPORT_LAYER_DEFAULT,
    };

    enum DebugViewportLayer : u8
    {
        DEBUG_VIEWPORT_LAYER_DEFAULT,
    };

    enum HUDViewportLayer : u8
    {
        HUD_VIEWPORT_LAYER_DEFAULT,
    };

    enum SortOrder : u8
    {
        SORT_FRONT_TO_BACK = 0,
        SORT_BACK_TO_FRONT
    };

    union
    {
        struct
        {
            u32 materialSortKey; // material sort key (contains states and pipeline setup infos as a bitfield)

            u16 depth; // half float depth for distance sorting
            SortOrder sortOrder : 5; // front to back or back to front (opaque or transparent)
            u8 viewportLayer : 3;

            Layer layer : 4;
            u8 viewportId : 3; // viewport dispatch index (should be managed by the builder)
            u8 __PADDING__ : 1;
        } bitfield;
        uint64_t value;
    };
};

struct DrawCommandInfos
{
    struct InstanceData
    {
        dkMat4x4f   ModelMatrix;
        u32         EntityIdentifier;
        f32         LodDitheringAlpha;
        u32         __PADDING__[2];
    };

    const Material*             material; // Geom cmd
    const BufferBinding*        vertexBuffers;
    const BufferBinding*        indiceBuffer;
    u32                         indiceBufferOffset; // unused if indice buffer is null
    u32                         indiceBufferCount; // same as above
    u32                         vertexBufferCount;
    f32                         alphaDitheringValue; // 0..1 (1.0f if disabled)
    u32                         instanceCount; // 0 or 1 implicitly disable instancing
    const InstanceData*         modelMatrix; // Points to a single matrix or an array of matrix (if instancing is used)
    
    u8                          useShortIndices : 1;
};

struct DrawCmd
{
    DrawCommandKey      key;
    DrawCommandInfos    infos;

    static bool SortFrontToBack( const DrawCmd& cmd1, const DrawCmd& cmd2 )
    {
        return ( cmd1.key.value < cmd2.key.value );
    }

    static bool SortBackToFront( const DrawCmd& cmd1, const DrawCmd& cmd2 )
    {
        return ( cmd1.key.value > cmd2.key.value );
    }
};
