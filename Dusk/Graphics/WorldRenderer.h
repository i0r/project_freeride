/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Rendering/CommandList.h>
#include <Maths/Matrix.h>

#include "Material.h"

class LightGrid;
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
class TextRenderingModule;
class AutomaticExposureModule;
class GlareRenderModule;
class LineRenderingModule;
class FrameCompositionModule;
class AtmosphereRenderModule;
class WorldRenderModule;
class IBLUtilitiesModule;
class EnvironmentProbeStreaming;

struct CameraData;
struct Buffer;
struct Image;

using ResHandle_t = u32;

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
    const Buffer* const *       vertexBuffers;
    const Buffer*               indiceBuffer;
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

class WorldRenderer
{
public:
    // TODO Avoid exposing stuff like this...
    // We should be able to get rid of this shitty dependency once picking is correctly split.
    WorldRenderModule*       WorldRendering;

    // Return a pointer to the AtmosphereRenderModule instance. Shouldn't be useful outside the editor context.
    DUSK_INLINE AtmosphereRenderModule* getAtmosphereRenderingModule() const { return atmosphereRendering; }

    // Return a pointer to this renderer light grid.  Shouldn't be useful outside the editor context.
    DUSK_INLINE LightGrid*              getLightGrid() const { return lightGrid; }

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

    // Return a pointer to the wireframe material.
    const Material*  getWireframeMaterial() const;

    // Append the render passes to build the default world render pipeline.
    ResHandle_t      buildDefaultGraph( FrameGraph& frameGraph, const Material::RenderScenario scenario, const dkVec2f& viewportSize );

    // Return the virtual resource handle to the resolved depth buffer (or the regular depth buffer if multisampling is disabled).
    ResHandle_t      getResolvedDepth();

private:
    // The memory allocator owning this instance.
    BaseAllocator*   memoryAllocator;

    // Cache to precompute and store basic primitives (for debug or special stuff).
    PrimitiveCache*  primitiveCache;

    // Draw Command allocator.
    LinearAllocator* drawCmdAllocator;

    // The FrameGraph used to render the world.
    FrameGraph*      frameGraph;

    // Current frame draw commands.
    DrawCmd*         frameDrawCmds;

    // If true, RenderModules need to precompute its transistent resources for frame rendering.
    bool             needResourcePrecompute;

    // Material to render wireframe geometry (e.g. debug primitives).
    Material*        wireframeMaterial;

    // Default BRDF DFG LUT. Probably at the wrong place but idk where it should now.
    Image*           brdfDfgLut;

    // Manage light assignation/clustering in the world.
    LightGrid*       lightGrid;

    // Update dynamic environment probe and stream precomputed ones from the disk.
    EnvironmentProbeStreaming* environmentProbeStreaming;

    // Current frame resolved depth render target (if the frame had at least one depth render pass).
    ResHandle_t      resolvedDepth;

    // Frame composition render module (builds final image at the end of the post-fx pipeline).
    FrameCompositionModule* frameComposition;

    // Glare rendering module (physically-based bloom).
    GlareRenderModule* glareRendering;

    // Automatic exposure compute module (histogram based).
    AutomaticExposureModule* automaticExposure;

    // Atmosphere (sky + atmospheric fog) rendering module.
    AtmosphereRenderModule* atmosphereRendering;
};
