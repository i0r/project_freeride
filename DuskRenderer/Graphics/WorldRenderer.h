/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Rendering/CommandList.h>

class RenderDevice;
class ShaderCache;
class GraphicsAssetCache;
class PrimitiveCache;
class BaseAllocator;
class PoolAllocator;
class LinearAllocator;
class CommandList;
class FrameGraph;
class Material;
class VirtualFileSystem;
class BrunetonSkyRenderModule;
class TextRenderingModule;
class AutomaticExposureModule;
class GlareRenderModule;
class LineRenderingModule;

template <typename Precision, int RowCount, int ColumnCount>
struct Matrix;
using dkMat4x4f = Matrix<float, 4, 4>;

struct CameraData;
struct Buffer;

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
    Material*                   material; // Geom cmd
    const Buffer**              vertexBuffers;
    const Buffer*               indiceBuffer;
    u32                         indiceBufferOffset; // unused if indice buffer is null
    u32                         indiceBufferCount; // same as above
    u32                         vertexBufferCount;
    f32                         alphaDitheringValue; // 0..1 (1.0f if disabled)
    u32                         instanceCount; // 0 or 1 implicitly disable instancing
    const dkMat4x4f*            modelMatrix; // Points to a single matrix or an array of matrix (if instanciation is used)
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

class WorldRenderer
{
public:
    // TODO Avoid exposing stuff like this...
    BrunetonSkyRenderModule* BrunetonSky;
    AutomaticExposureModule* AutomaticExposure;
    TextRenderingModule*     TextRendering;
    GlareRenderModule*       GlareRendering;
    LineRenderingModule*     LineRendering;

public:
                     WorldRenderer( BaseAllocator* allocator );
                     WorldRenderer( WorldRenderer& ) = default;
                     ~WorldRenderer();

    void             destroy( RenderDevice* renderDevice );
    void             loadCachedResources( RenderDevice* renderDevice, ShaderCache* shaderCache, GraphicsAssetCache* graphicsAssetCache, VirtualFileSystem* virtualFileSystem );

    void             drawDebugSphere( CommandList& cmdList );

    void             drawWorld( RenderDevice* renderDevice, const f32 deltaTime );

    DrawCmd&         allocateDrawCmd();
    DrawCmd&         allocateSpherePrimitiveDrawCmd();

    // Prepare the FrameGraph instance for the next frame.
    FrameGraph&      prepareFrameGraph( const Viewport& viewport, const ScissorRegion& scissor, const CameraData* camera = nullptr );

private:
    // The memory allocator owning this instance.
    BaseAllocator*   memoryAllocator;
    PrimitiveCache*  primitiveCache;

    // Draw Command allocator.
    LinearAllocator* drawCmdAllocator;

    // The FrameGraph used to render the world.
    FrameGraph*      frameGraph;

    // If true, RenderModules need to precompute its transistent resources for frame rendering.
    bool             needResourcePrecompute;
};
